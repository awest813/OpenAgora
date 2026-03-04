#include "ZoneManager.hxx"
#include "LOG.hxx"
#include "../services/GameClock.hxx"
#include "../services/Randomizer.hxx"
#include "GameStates.hxx"
#include <SignalMediator.hxx>
#include <MapFunctions.hxx>

#include <algorithm>
#include <random>
#include <thread>

ZoneManager::ZoneManager()
{
  // Callbacks for setTileID
  SignalMediator::instance().registerCbSetTileID(Signal::slot(this, &ZoneManager::updatePlacedNodes));
  SignalMediator::instance().registerCbDemolish(Signal::slot(this, &ZoneManager::updateRemovedNodes));
  SignalMediator::instance().registerCbUpdatePower(Signal::slot(this, &ZoneManager::updatePower));

  GameClock::instance().addRealTimeClockTask(
      [this]()
      {
        update();
        spawnBuildings();
        return false;
      },
      1s, 1s);
}

void ZoneManager::update()
{
  // Vacate nodes (Demolish Buildings on zones)
  if (!m_nodesToVacate.empty())
  {
    for (auto nodeToVacate : m_nodesToVacate)
    {
      for (auto &zoneArea : m_zoneAreas)
      {
        if (zoneArea.isMemberOf(nodeToVacate))
        {
          zoneArea.setVacancy(nodeToVacate, true);
          break;
        }
      }
    }
    m_nodesToVacate.clear();
  }

  // Occupy nodes (building has been spawned on a zone node)
  if (!m_nodesToOccupy.empty())
  {
    for (auto nodeToOccupy : m_nodesToOccupy)
    {
      for (auto &zoneArea : m_zoneAreas)
      {
        if (zoneArea.isMemberOf(nodeToOccupy))
        {
          zoneArea.setVacancy(nodeToOccupy, false);
          break;
        }
      }
    }
    m_nodesToOccupy.clear();
  }

  // Add new nodes (zone has been placed)
  if (!m_nodesToAdd.empty())
  {
    for (auto nodeToAdd : m_nodesToAdd)
    {
      addZoneNodeToArea(nodeToAdd, m_zoneAreas);
    }
    m_nodesToAdd.clear();
  }

  // Remove nodes (Dezone on zone tiles)
  if (!m_nodesToRemove.empty())
  {
    for (auto m_nodeToRemove : m_nodesToRemove)
    {
      removeZoneNode(m_nodeToRemove);
    }
    m_nodesToRemove.clear();
  }
}

void ZoneManager::spawnBuildings()
{
  // A thread-local RNG is used to convert the growth-rate multiplier into a
  // per-area spawn probability without introducing determinism issues.
  // Combine random_device entropy with the thread ID hash for a better seed.
  static thread_local std::mt19937 rng{std::random_device{}() ^
    static_cast<unsigned>(std::hash<std::thread::id>{}(std::this_thread::get_id()))};
  std::uniform_real_distribution<float> dist{0.f, 1.f};

  for (auto &zoneArea : m_zoneAreas)
  {
    // check if there are any buildings to spawn, if not, do nothing.
    if (zoneArea.isVacant() && zoneArea.hasPowerSupply())
    {
      int occupied = 0;
      int free = 0;
      for (auto node : zoneArea)
      {
        if (node.occupied)
        {
          occupied++;
        }
        else
          free++;
      }

      // For rates below 1.0, skip this area with probability (1 – rate)
      // so that building growth slows proportionally.
      if (m_growthRateMultiplier < 1.f && dist(rng) > m_growthRateMultiplier)
        continue;

      zoneArea.spawnBuildings();

      // For rates above 1.0, give a bonus spawn attempt with probability
      // equal to the fractional excess (e.g. 1.10 → 10 % extra chance).
      if (m_growthRateMultiplier > 1.f && dist(rng) < (m_growthRateMultiplier - 1.f))
        zoneArea.spawnBuildings();
    }
  }
}

std::vector<int> ZoneManager::getAdjacentZoneAreas(const ZoneNode &zoneNode, std::vector<ZoneArea> &zoneAreas)
{
  std::vector<int> neighborZones;
  int i = 0;

  for (auto &zoneArea : zoneAreas)
  {
    if (zoneArea.getZone() == zoneNode.zoneType && (zoneArea.getZoneDensity() == zoneNode.zoneDensity) &&
        zoneArea.isWithinBoundaries(zoneNode.coordinate) && zoneArea.isNeighbor(zoneNode.coordinate))
    {
      neighborZones.push_back(i);
    }
    ++i;
  }

  return neighborZones;
}

void ZoneManager::addZoneNodeToArea(ZoneNode &zoneNode, std::vector<ZoneArea> &zoneAreas)
{
  auto zoneNeighbour = getAdjacentZoneAreas(zoneNode, zoneAreas);

  if (zoneNeighbour.empty())
  {
    // new zonearea
    zoneAreas.emplace_back(zoneNode);
  }
  else if (zoneNeighbour.size() == 1)
  {
    // add to this zone
    zoneAreas[zoneNeighbour[0]].addNode(zoneNode);
  }
  else
  {
    // merge zone areas
    ZoneArea &mergedZone = zoneAreas[zoneNeighbour[0]];
    mergedZone.addNode(zoneNode);

    for (int idx = 1; idx < zoneNeighbour.size(); ++idx)
    {
      mergeZoneAreas(mergedZone, zoneAreas[zoneNeighbour[idx]]);
    }

    for (int idx = zoneNeighbour.size() - 1; idx > 0; --idx)
    {
      zoneAreas.erase(zoneAreas.begin() + zoneNeighbour[idx]);
    }
  }
}

std::vector<ZoneArea> ZoneManager::rebuildZoneArea(ZoneArea &zoneArea)
{
  std::vector<ZoneArea> newZoneAreas;

  for (ZoneNode zoneNode : zoneArea)
  {
    addZoneNodeToArea(zoneNode, newZoneAreas);
  }

  return newZoneAreas;
}

void ZoneManager::removeZoneNode(Point coordinate)
{
  for (auto zoneIt = m_zoneAreas.begin(); zoneIt != m_zoneAreas.end(); zoneIt++)
  {
    if (zoneIt->isMemberOf(coordinate))
    {
      zoneIt->removeZoneNode(coordinate);

      if (zoneIt->size() == 0)
      {
        m_zoneAreas.erase(zoneIt);
      }
      else
      {
        auto zoneAreas = rebuildZoneArea(*zoneIt);
        assert(zoneAreas.size() > 0);
        // If zoneAreas size is 1, means zoneArea is still compact, nothing to be done

        if (zoneAreas.size() > 1)
        {
          m_zoneAreas.erase(zoneIt);
          m_zoneAreas.insert(m_zoneAreas.end(), zoneAreas.begin(), zoneAreas.end());
        }
      }

      break;
    }
  }
}

void ZoneManager::updatePower(const std::vector<PowerGrid> &powerGrid)
{
  for (const auto &grid : powerGrid)
  {
    for (auto &area : m_zoneAreas)
    {
      if (bool isGridConnected =
              area.end() !=
              std::find_if(area.begin(), area.end(), [grid](const ZoneNode &node) { return grid.isNeighbor(node.coordinate); });
          !isGridConnected)
        continue;
      if (grid.getPowerLevel() > 0)
      {
        area.setPowerSupply(true);
      }
      else
      {
        area.setPowerSupply(false);
      }
    }
  }
}

void ZoneManager::updateRemovedNodes(const MapNode *mapNode)
{

  switch (GameStates::instance().demolishMode)
  {
  case DemolishMode::DE_ZONE:
  {
    m_nodesToRemove.push_back(mapNode->getCoordinates());
    break;
  }
  case DemolishMode::DEFAULT:
  {
    if (!mapNode->getTileData(Layer::BUILDINGS))
    {
      m_nodesToVacate.push_back(mapNode->getCoordinates());
    }

    break;
  }
  default:
    break;
  }
}

void ZoneManager::updatePlacedNodes(const MapNode &mapNode)
{
  if (mapNode.getTileData(Layer::BUILDINGS) && mapNode.getTileData(Layer::ZONE))
  {
    m_nodesToOccupy.push_back(mapNode.getCoordinates());
  }

  // zone placed
  else if (mapNode.getTileData(Layer::ZONE))
  {
    ZoneNode nodeToAdd = {mapNode.getCoordinates(), mapNode.getTileData(Layer::ZONE)->zoneTypes[0],
                          mapNode.getTileData(Layer::ZONE)->zoneDensity[0]};
    m_nodesToAdd.push_back(nodeToAdd);
  }
}

void ZoneManager::reset()
{
  m_zoneAreas.clear();
  for (const auto &mapNode : MapFunctions::instance().getMapNodes())
  {
    if (mapNode.getTileData(Layer::ZONE))
    {
      ZoneNode nodeToAdd = {mapNode.getCoordinates(), mapNode.getTileData(Layer::ZONE)->zoneTypes[0],
                            mapNode.getTileData(Layer::ZONE)->zoneDensity[0]};
      m_nodesToAdd.push_back(nodeToAdd);
    }
  }
}

void ZoneManager::applyChurn(float churnFraction)
{
  if (churnFraction <= 0.f)
    return;

  for (auto &area : m_zoneAreas)
  {
    if (area.getZone() != +ZoneType::RESIDENTIAL)
      continue;

    // Collect all occupied nodes in this residential area
    std::vector<Point> occupied;
    for (const auto &node : area)
    {
      if (node.occupied)
        occupied.push_back(node.coordinate);
    }

    if (occupied.empty())
      continue;

    // Vacate a fraction of the occupied nodes (at least 1 if churn is active)
    const int toVacate = std::max(1, static_cast<int>(static_cast<float>(occupied.size()) * churnFraction));

    // Randomise which nodes to vacate so the effect spreads across the zone
    Randomizer::instance().shuffle(occupied.begin(), occupied.end());

    const int count = std::min(toVacate, static_cast<int>(occupied.size()));
    for (int i = 0; i < count; ++i)
    {
      area.setVacancy(occupied[i], true);
    }
  }
}

void ZoneManager::setGrowthRateMultiplier(float rate)
{
  // Clamp to a reasonable range: 0 stops all growth, >2 is excessive.
  m_growthRateMultiplier = std::max(0.f, std::min(2.f, rate));
}
