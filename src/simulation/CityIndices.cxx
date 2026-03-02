#include "CityIndices.hxx"

#include <algorithm>
#include <numeric>

void CityIndices::tick(const std::vector<const TileData *> &buildingTiles, int roadTileCount, int totalTileCount)
{
  m_previous = m_current;
  m_current.affordability = computeAffordability(buildingTiles);
  m_current.safety        = computeSafety(buildingTiles);
  m_current.jobs          = computeJobs(buildingTiles);
  m_current.commute       = computeCommute(roadTileCount, totalTileCount);
  m_current.pollution     = computePollution(buildingTiles, totalTileCount);
}

// ── Affordability ─────────────────────────────────────────────────────────────
// Weighted average of residential zone density.
// LOW density housing → affordable (e.g. suburban small houses).
// HIGH density housing → expensive (dense/gentrified neighbourhoods).
// Score: all-LOW → 100, all-MEDIUM → 50, all-HIGH → 0.
float CityIndices::computeAffordability(const std::vector<const TileData *> &tiles)
{
  int nLow = 0, nMed = 0, nHigh = 0;
  for (const TileData *td : tiles)
  {
    if (td->category != "Residential")
      continue;
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
  const int total = nLow + nMed + nHigh;
  if (total == 0)
    return 50.f;
  const float weightedDensity = static_cast<float>(nLow * 1 + nMed * 2 + nHigh * 3) / static_cast<float>(total);
  // weightedDensity in [1, 3] → affordability in [100, 0]
  return clamp100(100.f - (weightedDensity - 1.f) * 50.f);
}

// ── Safety ────────────────────────────────────────────────────────────────────
// Inverted average of crimeLevel and fireHazardLevel across all building tiles,
// plus a small industrial-presence penalty.
float CityIndices::computeSafety(const std::vector<const TileData *> &tiles)
{
  if (tiles.empty())
    return 70.f; // empty city is safe

  float totalCrime = 0.f, totalFire = 0.f;
  int indInhab = 0, totalInhab = 0;
  for (const TileData *td : tiles)
  {
    totalCrime += static_cast<float>(td->crimeLevel);
    totalFire  += static_cast<float>(td->fireHazardLevel);
    totalInhab += td->inhabitants;
    if (td->category == "Industrial")
      indInhab += td->inhabitants;
  }
  const float n = static_cast<float>(tiles.size());
  const float avgCrime = totalCrime / n;
  const float avgFire  = totalFire  / n;
  const float indFraction = (totalInhab > 0) ? static_cast<float>(indInhab) / static_cast<float>(totalInhab) : 0.f;
  return clamp100(100.f - avgCrime * 2.f - avgFire - indFraction * 30.f);
}

// ── Jobs / Economy ────────────────────────────────────────────────────────────
// Ratio of commercial + industrial inhabitant capacity to residential capacity.
// ~2:1 residential dominance → score ~50. Equal split → 100.
float CityIndices::computeJobs(const std::vector<const TileData *> &tiles)
{
  int resInhab = 0, comInhab = 0, indInhab = 0;
  for (const TileData *td : tiles)
  {
    if (td->category == "Residential")
      resInhab += td->inhabitants;
    else if (td->category == "Commercial")
      comInhab += td->inhabitants;
    else if (td->category == "Industrial")
      indInhab += td->inhabitants;
  }
  if (resInhab == 0 && comInhab == 0 && indInhab == 0)
    return 50.f;
  const float employmentRatio =
      static_cast<float>(comInhab + indInhab) / static_cast<float>(std::max(resInhab, 1));
  return clamp100(employmentRatio * 50.f);
}

// ── Commute ───────────────────────────────────────────────────────────────────
// Road coverage relative to total map size.
// 1 road tile per 20 map tiles (5 %) yields a perfect score.
float CityIndices::computeCommute(int roadCount, int totalTiles)
{
  if (totalTiles <= 0)
    return 50.f;
  return clamp100(static_cast<float>(roadCount) / static_cast<float>(totalTiles) * 20.f * 100.f);
}

// ── Pollution ─────────────────────────────────────────────────────────────────
// Inverted average pollution across the whole map.
// A single polluting tile in a large map barely affects the score;
// many polluters together push it toward 0.
float CityIndices::computePollution(const std::vector<const TileData *> &tiles, int totalTiles)
{
  if (totalTiles <= 0)
    return 100.f;
  float totalPollution = 0.f;
  for (const TileData *td : tiles)
    totalPollution += static_cast<float>(td->pollutionLevel);
  const float avgPollution = totalPollution / static_cast<float>(totalTiles);
  // avgPollution ≈ 80/totalTiles per heavy polluter; scale so avg ≈ 1 → score ≈ 0
  return clamp100(100.f - avgPollution * 1.25f);
}
