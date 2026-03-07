#include "ServiceStrainModel.hxx"

#include <algorithm>
#include <cctype>
#include <string>

float ServiceStrainModel::clamp100(float value)
{
  return value < 0.f ? 0.f : (value > 100.f ? 100.f : value);
}

std::string ServiceStrainModel::normalize(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

bool ServiceStrainModel::categoryContains(const TileData *tile, const char *needle)
{
  const std::string category = normalize(tile->category);
  return category.find(needle) != std::string::npos;
}

void ServiceStrainModel::reset()
{
  m_state = ServiceStrainState{};
}

void ServiceStrainModel::tick(const std::vector<const TileData *> &buildingTiles, int roadTileCount, int totalTileCount,
                              const CityIndicesData &indices)
{
  int residentialCapacity = 0;
  int safetyCapacity      = 0;
  int educationCapacity   = 0;
  int healthCapacity      = 0;

  for (const TileData *tile : buildingTiles)
  {
    if (tile->category == "Residential")
      residentialCapacity += tile->inhabitants;

    if (categoryContains(tile, "emergency"))
      safetyCapacity += std::max(1, tile->inhabitants);

    if (categoryContains(tile, "school") || categoryContains(tile, "education"))
      educationCapacity += std::max(1, tile->inhabitants);

    if (categoryContains(tile, "health") || categoryContains(tile, "hospital") || categoryContains(tile, "water"))
      healthCapacity += std::max(1, tile->inhabitants);
  }

  const float residentBase = static_cast<float>(std::max(1, residentialCapacity));
  const float roadCoverage = (totalTileCount > 0) ? static_cast<float>(roadTileCount) / static_cast<float>(totalTileCount) : 0.f;

  // Transit reliability combines commute quality and road network coverage.
  m_state.transitReliability = clamp100(indices.commute * 0.70f + roadCoverage * 100.f * 0.30f);

  const float safetyCoverage    = static_cast<float>(safetyCapacity) / residentBase;
  const float educationCoverage = static_cast<float>(educationCapacity) / residentBase;
  const float healthCoverage    = static_cast<float>(healthCapacity) / residentBase;

  // "Load/Stress" values: higher is worse.
  m_state.safetyCapacityLoad    = clamp100(100.f - safetyCoverage * 100.f + (100.f - indices.safety) * 0.35f);
  m_state.educationAccessStress = clamp100(100.f - educationCoverage * 100.f + (100.f - indices.jobs) * 0.15f);
  m_state.healthAccessStress    = clamp100(100.f - healthCoverage * 100.f + indices.pollution * 0.30f);
}
