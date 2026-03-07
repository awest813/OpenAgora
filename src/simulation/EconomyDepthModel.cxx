#include "EconomyDepthModel.hxx"

#include <algorithm>

float EconomyDepthModel::clamp100(float value)
{
  return value < 0.f ? 0.f : (value > 100.f ? 100.f : value);
}

void EconomyDepthModel::configure(const Config &config)
{
  m_config.debtStressScale       = std::max(1.f, config.debtStressScale);
  m_config.policyCostStressScale = std::max(1.f, config.policyCostStressScale);
  m_config.confidenceDebtPenalty = std::max(0.f, config.confidenceDebtPenalty);
}

void EconomyDepthModel::reset()
{
  m_state = EconomyDepthState{};
}

void EconomyDepthModel::tick(const std::vector<const TileData *> &buildingTiles, const CityIndicesData &indices,
                             float runningBalance, float policyExpenses, float approval)
{
  int residentialCapacity = 0;
  int commercialCapacity  = 0;
  int industrialCapacity  = 0;

  for (const TileData *tile : buildingTiles)
  {
    if (tile->category == "Residential")
      residentialCapacity += tile->inhabitants;
    else if (tile->category == "Commercial")
      commercialCapacity += tile->inhabitants;
    else if (tile->category == "Industrial")
      industrialCapacity += tile->inhabitants;
  }

  const float employmentRatio = static_cast<float>(commercialCapacity + industrialCapacity)
                              / static_cast<float>(std::max(1, residentialCapacity));

  // More jobs per resident lowers unemployment pressure.
  m_state.unemploymentPressure = clamp100(100.f - employmentRatio * 100.f);

  // Wage pressure rises when employment is tight and when jobs index is high.
  const float tightLaborFactor = 100.f - m_state.unemploymentPressure;
  m_state.wagePressure = clamp100(20.f + tightLaborFactor * 0.45f + indices.jobs * 0.25f + approval * 0.10f
                                  - indices.pollution * 0.08f);

  // Debt stress grows from negative balance and high monthly policy expenses.
  const float balanceStress = runningBalance < 0.f ? (-runningBalance / m_config.debtStressScale) : 0.f;
  const float spendStress   = policyExpenses / m_config.policyCostStressScale;
  m_state.debtStress        = clamp100(balanceStress + spendStress);

  // Confidence tracks mobility/safety/jobs/approval and drops under debt stress.
  m_state.businessConfidence =
      clamp100(indices.jobs * 0.35f + indices.commute * 0.20f + indices.safety * 0.20f + approval * 0.25f
               - m_state.debtStress * m_config.confidenceDebtPenalty);
}
