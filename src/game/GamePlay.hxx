#ifndef GAMEPLAY_HXX_
#define GAMEPLAY_HXX_

#include "ZoneManager.hxx"
#include "PowerManager.hxx"
#include "Singleton.hxx"

#include <MapNode.hxx>

/**
 * @brief Top-level gameplay orchestrator.
 *
 * Owns ZoneManager and PowerManager (existing subsystems) and drives the new
 * simulation layer each game-month via runMonthlySimulationTick(). The caller
 * (Map / Game) is responsible for scheduling the tick via GameClock.
 */
class GamePlay
{
public:
  GamePlay() = default;

  void resetManagers();

  ZoneManager &getZoneManager() { return m_ZoneManager; }

  /**
   * @brief Run one game-month simulation tick.
   *
   * Reads all building tiles from the supplied map nodes, then drives:
   *   - CityIndices (aggregation)
   *   - AffordabilityModel (rent/income/displacement) – if flag enabled
   *   - PolicyEngine (apply active policy effects) – if flag enabled
   *   - GovernanceSystem (approval, events, checkpoints) – if flag enabled
   *
   * @param mapNodes The full flat vector of map nodes to query tile data from.
   */
  void runMonthlySimulationTick(const std::vector<MapNode> &mapNodes);

private:
  ZoneManager m_ZoneManager;
  PowerManager m_PowerManager;
};

#endif
