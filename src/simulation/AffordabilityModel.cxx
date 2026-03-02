#include "AffordabilityModel.hxx"

#include <algorithm>
#include <cmath>

void AffordabilityModel::configure(const Config &config)
{
  m_config.affordabilityThreshold = clamp100(config.affordabilityThreshold);
  m_config.displacementRate       = std::max(0.f, config.displacementRate);
  m_config.recoveryRate           = std::max(0.f, config.recoveryRate);
  m_config.churnThreshold         = clamp100(config.churnThreshold);
  m_config.maxChurnPerTick        = std::max(0.f, std::min(1.f, config.maxChurnPerTick));
}

void AffordabilityModel::reset()
{
  m_state      = AffordabilityState{};
  m_churnActive = false;
  m_churnRate   = 0.f;
}

// ── Private helpers ───────────────────────────────────────────────────────────

// medianRent: weighted by zone density.
//   LOW density → cheap (weight 1), MEDIUM → moderate (weight 2), HIGH → expensive (weight 3).
//   Score range [0, 100] where 0 = only low-density cheap housing, 100 = all luxury high-density.
float AffordabilityModel::computeMedianRent(const std::vector<const TileData *> &tiles)
{
  int nLow = 0, nMed = 0, nHigh = 0;
  float totalUpkeep = 0.f;
  int resTiles = 0;

  for (const TileData *td : tiles)
  {
    if (td->category != "Residential")
      continue;

    ++resTiles;
    totalUpkeep += static_cast<float>(std::max(0, td->upkeepCost));

    for (const auto &d : td->zoneDensity)
    {
      if (d == +ZoneDensity::LOW)
        ++nLow;
      else if (d == +ZoneDensity::MEDIUM)
        ++nMed;
      else if (d == +ZoneDensity::HIGH)
        ++nHigh;
    }
  }

  if (resTiles == 0)
    return 20.f; // sparse city – cheap

  const int total = nLow + nMed + nHigh;
  // density factor [1, 3] → rent factor [0, 100]
  const float densityFactor = (total > 0)
                                  ? static_cast<float>(nLow + nMed * 2 + nHigh * 3) / static_cast<float>(total)
                                  : 1.f;
  const float densityRent = (densityFactor - 1.f) * 50.f; // [0, 100]

  // upkeep surcharge: add up to 30 points for high-upkeep tiles
  const float avgUpkeep = totalUpkeep / static_cast<float>(resTiles);
  const float upkeepRent = std::min(30.f, avgUpkeep / 50.f); // 50 upkeep/tile → +30

  return clamp100(densityRent + upkeepRent);
}

// medianIncome: derived from LOW-density-dominated residential = poor,
// HIGH-density residential = wealthier workers (commercial hub adjacency).
// Also boosted if there are many commercial/industrial jobs nearby.
float AffordabilityModel::computeMedianIncome(const std::vector<const TileData *> &tiles)
{
  int resInhab = 0, comInhab = 0, indInhab = 0;
  int nLow = 0, nMed = 0, nHigh = 0;

  for (const TileData *td : tiles)
  {
    if (td->category == "Residential")
    {
      resInhab += td->inhabitants;
      for (const auto &d : td->zoneDensity)
      {
        if (d == +ZoneDensity::LOW)
          ++nLow;
        else if (d == +ZoneDensity::MEDIUM)
          ++nMed;
        else if (d == +ZoneDensity::HIGH)
          ++nHigh;
      }
    }
    else if (td->category == "Commercial")
    {
      comInhab += td->inhabitants;
    }
    else if (td->category == "Industrial")
    {
      indInhab += td->inhabitants;
    }
  }

  // Base income from density tier distribution
  const int totalDensity = nLow + nMed + nHigh;
  float baseIncome = 40.f; // neutral baseline
  if (totalDensity > 0)
  {
    // LOW density → working class (~30), MEDIUM → middle (~50), HIGH → mixed (~60)
    const float weightedTier =
        static_cast<float>(nLow * 30 + nMed * 50 + nHigh * 60) / static_cast<float>(totalDensity);
    baseIncome = weightedTier;
  }

  // Employment surplus boosts income: more jobs than residents → upward pressure
  const float employmentRatio =
      (resInhab > 0) ? static_cast<float>(comInhab + indInhab) / static_cast<float>(resInhab) : 0.f;
  const float jobsBonus = std::min(20.f, employmentRatio * 10.f);

  return clamp100(baseIncome + jobsBonus);
}

float AffordabilityModel::computeLandValueProxy(const std::vector<const TileData *> &tiles)
{
  if (tiles.empty())
    return 30.f;

  int comCount = 0, highCount = 0, total = 0;
  for (const TileData *td : tiles)
  {
    if (td->category == "Residential" || td->category == "Commercial")
    {
      ++total;
      if (td->category == "Commercial")
        ++comCount;
      for (const auto &d : td->zoneDensity)
        if (d == +ZoneDensity::HIGH)
          ++highCount;
    }
  }

  if (total == 0)
    return 30.f;

  const float comFraction  = static_cast<float>(comCount)  / static_cast<float>(total);
  const float highFraction = static_cast<float>(highCount) / static_cast<float>(total);
  return clamp100(30.f + comFraction * 40.f + highFraction * 30.f);
}

// ── tick() ────────────────────────────────────────────────────────────────────

void AffordabilityModel::tick(const std::vector<const TileData *> &buildingTiles, float policyAffordabilityBonus)
{
  m_state.medianRent   = computeMedianRent(buildingTiles);
  m_state.medianIncome = computeMedianIncome(buildingTiles);
  m_state.landValueProxy = computeLandValueProxy(buildingTiles);

  // Core formula (DESIGN.md §4.2):
  //   affordabilityIndex = clamp(100 - (rent/income)*50 - displacement*0.3 + policyBonus, 0, 100)
  const float incomeBase = std::max(1.f, m_state.medianIncome);
  const float rentRatio  = m_state.medianRent / incomeBase;
  const float rawIndex   = 100.f - rentRatio * 50.f
                                 - m_state.displacementPressure * 0.3f
                                 + policyAffordabilityBonus;
  m_state.affordabilityIndex = clamp100(rawIndex);

  // Update displacement pressure
  if (m_state.affordabilityIndex < m_config.affordabilityThreshold)
  {
    m_state.displacementPressure =
        clamp100(m_state.displacementPressure + m_config.displacementRate);
  }
  else
  {
    m_state.displacementPressure =
        std::max(0.f, m_state.displacementPressure - m_config.recoveryRate);
  }

  // Population churn
  m_churnActive = m_state.displacementPressure > m_config.churnThreshold;
  if (m_churnActive)
  {
    const float excess = (m_state.displacementPressure - m_config.churnThreshold)
                       / (100.f - m_config.churnThreshold);
    m_churnRate = excess * m_config.maxChurnPerTick;
  }
  else
  {
    m_churnRate = 0.f;
  }
}
