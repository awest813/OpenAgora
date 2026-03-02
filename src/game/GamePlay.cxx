#include "GamePlay.hxx"

#include "AffordabilityModel.hxx"
#include "CityIndices.hxx"
#include "GovernanceSystem.hxx"
#include "PolicyEngine.hxx"
#include "../services/FeatureFlags.hxx"
#include "LOG.hxx"
#include "enums.hxx"

void GamePlay::resetManagers()
{
  m_ZoneManager.reset();
  m_PowerManager.reset();
}

void GamePlay::runMonthlySimulationTick(const std::vector<MapNode> &mapNodes)
{
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

  // ── PolicyEngine ───────────────────────────────────────────────────────────
  // When the json_content_pipeline flag is off, policies are not loaded and
  // tick() is a no-op. The actual policy effects on affordability are computed
  // inside the AffordabilityModel block below via a probe pass.
  if (flags.jsonContentPipeline() && !flags.affordabilitySystem())
  {
    // If the affordability system is off but policies are on, apply effects
    // directly to the indices snapshot so they still influence governance.
    AffordabilityState probeAff{};
    PolicyEngine::instance().tick(probeAff, indices);
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
      PolicyEngine::instance().tick(probe, dummyIndices);
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
  }
}
