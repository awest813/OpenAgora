#ifndef ZONEMANAGER_HXX_
#define ZONEMANAGER_HXX_

#include "ZoneArea.hxx"
#include "PowerGrid.hxx"
#include <MapNode.hxx>

class ZoneManager
{
public:
  ZoneManager();

  void reset();

  /**
   * @brief Apply population churn by vacating a fraction of occupied residential zone nodes.
   *
   * When AffordabilityModel reports active displacement pressure this is called
   * from GamePlay::runMonthlySimulationTick() to vacate a fraction of occupied
   * residential nodes, allowing ZoneManager to respawn (lower-density) buildings
   * over time.
   *
   * @param churnFraction Fraction [0, 1] of occupied residential nodes to vacate this tick.
   */
  void applyChurn(float churnFraction);

  /**
   * @brief Set the zone growth rate multiplier driven by governance approval.
   *
   * Values below 1.0 slow building spawning (fraction of spawn attempts are
   * skipped).  Values above 1.0 give some zone areas a bonus spawn attempt
   * each tick.  The multiplier is applied probabilistically per-area inside
   * spawnBuildings().
   *
   * Approval ranges from DESIGN.md §4.5:
   *   < 30   → 0.90  (growth rate × 0.90)
   *   30–70  → 1.00  (neutral)
   *   70–85  → 1.10  (growth rate × 1.10)
   *   ≥ 85   → 1.20  (growth rate × 1.20)
   *
   * @param rate Multiplier in [0, 2]. Out-of-range values are clamped.
   */
  void setGrowthRateMultiplier(float rate);

private:
  /**
   * @brief Spawn Buildings on the gathered tileMap
   * 
   */
  void spawnBuildings();

  /**
   * @brief Process previously cached nodes to update
   * 
   */
  void update();

  /**
   * @brief Removes a zonenode
   * 
   * @param coordinate coordinate of the zone to remove
   */
  void removeZoneNode(Point coordinate);

  /**
   * @brief get a list of neighboring zoneareas for a zoneNode
   * 
   * @param zoneNode the zoneNode we need neighboring areas for
   * @param zoneAreas a vector of areas that apply
   * @return a list of neighboring zoneareas for a zoneNode
   */
  std::vector<int> getAdjacentZoneAreas(const ZoneNode &zoneNode, std::vector<ZoneArea> &zoneAreas);

  /**
   * @brief Adds a zoneNode to a given area
   * 
   * @param zoneNode node to add
   * @param zoneAreas all zoneAreas
   */
  void addZoneNodeToArea(ZoneNode &zoneNode, std::vector<ZoneArea> &zoneAreas);

  /**
   * @brief rebuild a certain zone area
   * 
   * @param zoneArea the area to rebuild
   * @return rebuilt zone area
   */
  std::vector<ZoneArea> rebuildZoneArea(ZoneArea &zoneArea);

  void updatePower(const std::vector<PowerGrid> &powerGrid);

  void updateRemovedNodes(const MapNode *mapNode);

  void updatePlacedNodes(const MapNode &mapNode);

  std::vector<ZoneArea> m_zoneAreas;  /// All zoneAreas
  std::vector<ZoneNode> m_nodesToAdd; /// All zoneAreas
  std::vector<Point> m_nodesToOccupy; /// All zoneAreas
  std::vector<Point> m_nodesToVacate; /// All zoneAreas
  std::vector<Point> m_nodesToRemove; /// All zoneAreas
  float m_growthRateMultiplier = 1.0f; ///< Governance-driven spawn rate modifier
};

#endif