#ifndef CITY_INDICES_HXX_
#define CITY_INDICES_HXX_

#include "Singleton.hxx"
#include "tileData.hxx"
#include <vector>

/// Five city-wide scalar indices, each in [0, 100]. Higher is always better.
struct CityIndicesData
{
  float affordability = 50.f; ///< Housing cost vs. density (100 = very affordable)
  float safety        = 50.f; ///< Inverted crime + fire hazard (100 = very safe)
  float jobs          = 50.f; ///< Commercial/industrial capacity relative to residents
  float commute       = 50.f; ///< Road infrastructure relative to map size
  float pollution     = 50.f; ///< Inverted pollution (100 = pristine)
};

/**
 * @class CityIndices
 * @brief Aggregates per-tile TileData into five city-wide scalar indices.
 *
 * Pure C++17 – no SDL headers.  Called monthly via GameClock.
 * The CityIndicesPanel reads current() and previous() each frame to render
 * coloured progress bars and trend arrows.
 */
class CityIndices : public Singleton<CityIndices>
{
public:
  friend Singleton<CityIndices>;

  /**
   * @brief Recompute all indices from the supplied tile snapshots.
   * @param buildingTiles Non-null TileData* from each occupied BUILDINGS layer node.
   * @param roadTileCount Number of map nodes with an occupied ROAD layer.
   * @param totalTileCount Total tile count in the map (columns × rows).
   */
  void tick(const std::vector<const TileData *> &buildingTiles, int roadTileCount, int totalTileCount);

  const CityIndicesData &current()  const { return m_current; }
  const CityIndicesData &previous() const { return m_previous; }

private:
  CityIndices() = default;

  CityIndicesData m_current;
  CityIndicesData m_previous;

  static float clamp100(float x) { return x < 0.f ? 0.f : (x > 100.f ? 100.f : x); }

  static float computeAffordability(const std::vector<const TileData *> &tiles);
  static float computeSafety(const std::vector<const TileData *> &tiles);
  static float computeJobs(const std::vector<const TileData *> &tiles);
  static float computeCommute(int roadCount, int totalTiles);
  static float computePollution(const std::vector<const TileData *> &tiles, int totalTiles);
};

#endif // CITY_INDICES_HXX_
