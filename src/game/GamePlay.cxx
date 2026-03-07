#include "GamePlay.hxx"

#include "AffordabilityModel.hxx"
#include "BudgetSystem.hxx"
#include "CityIndices.hxx"
#include "EconomyDepthModel.hxx"
#include "GovernanceSystem.hxx"
#include "PolicyEngine.hxx"
#include "ServiceStrainModel.hxx"
#include "SimulationContext.hxx"
#include "../services/FeatureFlags.hxx"
#include "LOG.hxx"
#include "enums.hxx"

#include <algorithm>

void GamePlay::resetManagers()
{
  m_ZoneManager.reset();
  m_PowerManager.reset();
}

void GamePlay::runMonthlySimulationTick(const std::vector<MapNode> &mapNodes)
{
  SimulationContext::instance().advanceMonth();
  SimulationContext::instance().mutableData().growthRateModifier = 1.f;

  // ── Collect tile snapshots ─────────────────────────────────────────────────
  std::vector<const TileData *> buildingTiles;
  int roadTileCount  = 0;
  const int totalTiles = static_cast<int>(mapNodes.size());

  buildingTiles.reserve(totalTiles / 4);

  for (const auto &node : mapNodes)
  {
    const TileData *buildingData = node.getTileData(Layer::BUILDINGS);
    if (buildingData)
      buildingTiles.push_back(buildingData);

    if (node.getTileData(Layer::ROAD))
      ++roadTileCount;
  }

  const FeatureFlags &flags = FeatureFlags::instance();

  // ── CityIndices (always runs when dashboard flag is on) ────────────────────
  if (flags.cityIndicesDashboard() || flags.governanceLayer())
  {
    CityIndices::instance().tick(buildingTiles, roadTileCount, totalTiles);
  }

  CityIndicesData indices = CityIndices::instance().current();
  float policyExpenses = 0.f;
  float approval = 50.f;

  // ── PolicyEngine ───────────────────────────────────────────────────────────
  // When the json_content_pipeline flag is off, policies are not loaded and
  // tick() is a no-op. The actual policy effects on affordability are computed
  // inside the AffordabilityModel block below via a probe pass.
  if (flags.jsonContentPipeline() && !flags.affordabilitySystem())
  {
    // If the affordability system is off but policies are on, apply effects
    // directly to the indices snapshot so they still influence governance.
    AffordabilityState probeAff{};
    PolicyEngine::instance().tick(probeAff, indices, &SimulationContext::instance().mutableData());
  }

  // ── AffordabilityModel ─────────────────────────────────────────────────────
  if (flags.affordabilitySystem())
  {
    // Compute net affordability bonus from active policies by doing a
    // side-effect-free probe: apply policy effects to a throwaway copy and
    // measure the delta on the affordability field.
    float policyAffordabilityBonus = 0.f;
    if (flags.jsonContentPipeline())
    {
      AffordabilityState probe = AffordabilityModel::instance().state();
      CityIndicesData dummyIndices = indices;
      SimulationContextData contextProbe = SimulationContext::instance().data();
      PolicyEngine::instance().tick(probe, dummyIndices, &contextProbe);
      policyAffordabilityBonus =
          probe.affordabilityIndex - AffordabilityModel::instance().state().affordabilityIndex;
    }

    AffordabilityModel::instance().tick(buildingTiles, policyAffordabilityBonus);

    // Propagate the model's refined affordability index back into CityIndices
    CityIndices::instance().overrideAffordability(AffordabilityModel::instance().state().affordabilityIndex);
    indices = CityIndices::instance().current();
  }

  // ── GovernanceSystem ───────────────────────────────────────────────────────
  if (flags.governanceLayer())
  {
    GovernanceSystem::instance().tickMonth(indices);

    // Apply approval-driven zone growth rate (DESIGN.md §4.5).
    approval = GovernanceSystem::instance().approval();
    float growthRate = 1.0f;
    if (approval >= 85.f)
      growthRate = 1.20f;
    else if (approval >= 70.f)
      growthRate = 1.10f;
    else if (approval < 30.f)
      growthRate = 0.90f;

    SimulationContextData &ctx = SimulationContext::instance().mutableData();
    ctx.growthRateModifier = growthRate;
    ctx.approvalMultiplier = (approval > 70.f) ? 1.10f : ((approval < 30.f) ? 0.80f : 1.00f);
    ctx.taxEfficiency = GovernanceSystem::instance().taxEfficiencyMultiplier() * ctx.approvalMultiplier;

    if (flags.affordabilitySystem())
    {
      AffordabilityState &affState = AffordabilityModel::instance().mutableState();
      affState.medianIncome = std::max(0.f, std::min(100.f, affState.medianIncome * GovernanceSystem::instance().incomeModifier()));
    }
  }

  // ── BudgetSystem ───────────────────────────────────────────────────────────
  if (flags.budgetSystem())
  {
    // Sum inhabitants across all building tiles for tax base
    int totalInhabitants = 0;
    for (const auto *tile : buildingTiles)
      totalInhabitants += tile->inhabitants;

    // Determine policy expenses: run the real tick now (post-affordability)
    if (flags.jsonContentPipeline())
    {
      AffordabilityState affCopy = AffordabilityModel::instance().state();
      CityIndicesData idxCopy = indices;
      policyExpenses = static_cast<float>(PolicyEngine::instance().tick(affCopy, idxCopy,
                                                                        &SimulationContext::instance().mutableData()));
    }

    approval = flags.governanceLayer() ? GovernanceSystem::instance().approval() : 50.f;
    const float budgetApproval =
        flags.governanceLayer()
            ? std::max(0.f, std::min(100.f, approval * GovernanceSystem::instance().taxEfficiencyMultiplier()))
            : approval;
    BudgetSystem::instance().tick(totalInhabitants, policyExpenses, budgetApproval);
  }

  if (flags.governanceLayer() || flags.jsonContentPipeline())
  {
    m_ZoneManager.setGrowthRateMultiplier(SimulationContext::instance().data().growthRateModifier);
  }

  // ── EconomyDepthModel ──────────────────────────────────────────────────────
  if (flags.economyDepthModel())
  {
    const float runningBalance = flags.budgetSystem() ? BudgetSystem::instance().currentBalance() : 0.f;
    EconomyDepthModel::instance().tick(buildingTiles, indices, runningBalance, policyExpenses, approval);

    const EconomyDepthState &economy = EconomyDepthModel::instance().state();
    SimulationContextData &ctx = SimulationContext::instance().mutableData();
    ctx.unemploymentPressure = economy.unemploymentPressure;
    ctx.wagePressure         = economy.wagePressure;
    ctx.businessConfidence   = economy.businessConfidence;
    ctx.debtStress           = economy.debtStress;
  }

  // ── ServiceStrainModel ─────────────────────────────────────────────────────
  if (flags.serviceStrainModel())
  {
    ServiceStrainModel::instance().tick(buildingTiles, roadTileCount, totalTiles, indices);

    const ServiceStrainState &services = ServiceStrainModel::instance().state();
    SimulationContextData &ctx = SimulationContext::instance().mutableData();
    ctx.transitReliability    = services.transitReliability;
    ctx.safetyCapacityLoad    = services.safetyCapacityLoad;
    ctx.educationAccessStress = services.educationAccessStress;
    ctx.healthAccessStress    = services.healthAccessStress;
  }

  // ── Population churn ───────────────────────────────────────────────────────
  if (flags.affordabilitySystem() && AffordabilityModel::instance().populationChurnActive())
  {
    m_ZoneManager.applyChurn(AffordabilityModel::instance().churnRate());
  }
}
