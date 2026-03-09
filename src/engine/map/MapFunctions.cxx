#include "MapFunctions.hxx"
#include <PointFunctions.hxx>
#include <unordered_map>
#include "../../services/Randomizer.hxx"
#include "common/JsonSerialization.hxx"
#include <Constants.hxx>
#include <Filesystem.hxx>
#include <Camera.hxx>
#include <ResourcesManager.hxx>
#include <MapLayers.hxx>
#include <isoMath.hxx>
#include <SignalMediator.hxx>
#include <AffordabilityModel.hxx>
#include <BudgetSystem.hxx>
#include <EconomyDepthModel.hxx>
#include <GovernanceSystem.hxx>
#include <PolicyEngine.hxx>
#include <ServiceStrainModel.hxx>
#include <SimulationContext.hxx>

#include <set>
#include <queue>
#include <unordered_set>

MapFunctions::MapFunctions() { SignalMediator::instance().registerCbSaveGame(Signal::slot(this, &MapFunctions::saveMapToFile)); }

bool MapFunctions::updateHeight(Point coordinate, const bool elevate)
{
  if (getMapNode(coordinate).changeHeight(elevate))
  {
    for (const auto neighbor : PointFunctions::getNeighbors(coordinate, false))
    {
      if (getMapNode(neighbor).isLayerOccupied(Layer::ZONE))
      {
        getMapNode(neighbor).demolishLayer(Layer::ZONE);
      }
    }

    return true;
  }

  return false;
}

void MapFunctions::changeHeight(const Point &isoCoordinates, const bool elevate)
{
  std::vector<Point> nodesToUpdate{isoCoordinates};
  std::vector<Point> neighorCoordinates = PointFunctions::getNeighbors(isoCoordinates, true);

  if (updateHeight(isoCoordinates, elevate))
  {
    // If lowering node height, than all nodes around should be lowered to be on same height with the central one.
    if (!elevate)
    {
      const int centerHeight = getMapNode(isoCoordinates).getCoordinates().height;

      for (const Point &neighborCoord : neighorCoordinates)
      {
        MapNode &neighborNode = getMapNode(neighborCoord);
        if (centerHeight < neighborNode.getCoordinates().height)
        {
          neighborNode.changeHeight(false);
          nodesToUpdate.push_back(neighborNode.getCoordinates());
        }
      }
    }
    demolishNode(neighorCoordinates);
    updateNodeNeighbors(nodesToUpdate);
  }
}

void MapFunctions::levelHeight(const Point &startCoordinate, const std::vector<Point> levelArea)
{
  const MapNode &startNode = getMapNode(startCoordinate);
  int initialHeight = startCoordinate.height;

  /* If the initial node is sloped, check if a majority of it's neighbors,
	* not counting other sloped nodes are elevated.
	* If so, the initial node should be regarded as elevated too,
	* i.e. the height should be incremented.
	* This seems to yield the most intuitive behavior on slopes.
	*/
  if (startNode.isSlopeNode())
  {
    char directNeighbors =
        NeighborNodesPosition::LEFT | NeighborNodesPosition::TOP | NeighborNodesPosition::RIGHT | NeighborNodesPosition::BOTTOM;
    char elBitmask = startNode.getElevationBitmask();

    int nonSelectedNeighborsCount = 0;
    int elevatedNonSelectedNeighborsCount = 0;

    for (Point neighbor : PointFunctions::getNeighbors(startCoordinate, false))
    {
      const NeighborNodesPosition neighborPosToOrigin = PointFunctions::getNeighborPositionToOrigin(neighbor, startCoordinate);
      const bool isSloped = getMapNode(neighbor).isSlopeNode();

      // Only look at non-sloped direct neighbors not in the selection.
      if (!isSloped && (neighborPosToOrigin & directNeighbors) > 0 &&
          std::find(levelArea.begin(), levelArea.end(), neighbor) == levelArea.end())
      {
        nonSelectedNeighborsCount++;

        if ((neighborPosToOrigin & elBitmask) > 0)
          elevatedNonSelectedNeighborsCount++;
      }
    }

    if (elevatedNonSelectedNeighborsCount * 2 > nonSelectedNeighborsCount)
      // No need to check max height since there are elevated neighbors.
      initialHeight++;
  }

  std::vector<Point> neighborsToLower;

  for (const Point &levelPoint : levelArea)
  {
    // If a node gets lowered, save all it's neighbors to be lowered aswell.
    // We have to get the node first, since the coordinates in the area are generated with height=0
    if (getMapNode(levelPoint).getCoordinates().height > initialHeight)
    {
      // This possibly stores nodes that have already been processed for another round.
      // It's faster than checking if each node is in levelArea first though.
      std::vector<Point> neighbors = PointFunctions::getNeighbors(levelPoint, false);
      neighborsToLower.insert(neighborsToLower.end(), neighbors.begin(), neighbors.end());
    }

    Point newCoordinates = Point(levelPoint.x, levelPoint.y, levelPoint.z, initialHeight);
    MapNode &levelNode = getMapNode(levelPoint);
    levelNode.setCoordinates(newCoordinates);
  }

  for (const Point &levelPoint : neighborsToLower)
  {
    Point newCoordinates = Point(levelPoint.x, levelPoint.y, levelPoint.z, initialHeight);
    MapNode &levelNode = getMapNode(levelPoint);
    levelNode.setCoordinates(newCoordinates);
  }

  updateNodeNeighbors(levelArea);
  updateNodeNeighbors(neighborsToLower);
}

void MapFunctions::updateNodeNeighbors(const std::vector<Point> &nodes)
{
  // those bitmask combinations require the tile to be elevated.
  constexpr unsigned char elevateTileComb[] = {
      NeighborNodesPosition::TOP | NeighborNodesPosition::BOTTOM,
      NeighborNodesPosition::LEFT | NeighborNodesPosition::RIGHT,
      NeighborNodesPosition::TOP_LEFT | NeighborNodesPosition::RIGHT | NeighborNodesPosition::BOTTOM,
      NeighborNodesPosition::TOP_RIGHT | NeighborNodesPosition::LEFT | NeighborNodesPosition::BOTTOM,
      NeighborNodesPosition::BOTTOM_LEFT | NeighborNodesPosition::RIGHT | NeighborNodesPosition::TOP,
      NeighborNodesPosition::BOTTOM_RIGHT | NeighborNodesPosition::LEFT | NeighborNodesPosition::TOP};

  std::vector<Point> nodesToBeUpdated;
  // ⚡ Bolt: Cache optimization - unordered_map provides O(1) lookup instead of O(log N) for integer keys.
  // This function is a hot path during terrain modification.
  std::unordered_map<int, std::vector<Point>> nodeCache;
  std::queue<Point> updatedNodes;
  std::unordered_set<Point> nodesToElevateSet;
  std::vector<Point> nodesToElevate;
  std::vector<Point> nodesToDemolish;

  for (auto currentNode : nodes)
  {
    updatedNodes.push(currentNode);

    while (!updatedNodes.empty() || !nodesToElevate.empty())
    {
      while (!updatedNodes.empty())
      {
        const Point pHeighChangedNode = getMapNode(updatedNodes.front()).getCoordinates();
        updatedNodes.pop();
        const int tileHeight = pHeighChangedNode.height;

        if (nodeCache.count(pHeighChangedNode.toIndex()) == 0)
        {
          nodeCache[pHeighChangedNode.toIndex()] = PointFunctions::getNeighbors(pHeighChangedNode, false);
        }

        if (nodesToElevateSet.insert(pHeighChangedNode).second)
        {
          nodesToElevate.push_back(pHeighChangedNode);
        }

        for (auto neighbor : nodeCache[pHeighChangedNode.toIndex()])
        {
          // get real coords that include height
          const Point neighborCoords = getMapNode(neighbor).getCoordinates();
          const int heightDiff = tileHeight - neighborCoords.height;

          if (nodeCache.count(neighborCoords.toIndex()) == 0)
          {
            nodeCache[neighborCoords.toIndex()] = PointFunctions::getNeighbors(neighborCoords, false);
          }

          if (nodesToElevateSet.insert(neighborCoords).second)
          {
            nodesToElevate.push_back(neighborCoords);
          }

          if (std::abs(heightDiff) > 1)
          {
            updatedNodes.push(neighborCoords);
            updateHeight(neighborCoords, (heightDiff > 1) ? true : false);
          }
        }
      }

      while (updatedNodes.empty() && !nodesToElevate.empty())
      {
        Point nodeToElevate = nodesToElevate.back();
        nodesToBeUpdated.push_back(nodeToElevate);
        nodesToElevate.pop_back();
        nodesToElevateSet.erase(nodeToElevate);

        if (nodeCache.count(nodeToElevate.toIndex()) == 0)
        {
          nodeCache[nodeToElevate.toIndex()] = PointFunctions::getNeighbors(nodeToElevate, false);
        }
        const unsigned char elevationBitmask = getElevatedNeighborBitmask(nodeToElevate);

        if (elevationBitmask != getMapNode(nodeToElevate).getElevationBitmask())
        {
          nodesToDemolish.push_back(nodeToElevate);
          getMapNode(nodeToElevate).setElevationBitMask(elevationBitmask);
        }

        for (const auto &elBitMask : elevateTileComb)
        {
          if ((elevationBitmask & elBitMask) == elBitMask)
          {
            updateHeight(nodeToElevate, true);
            updatedNodes.push(getMapNode(nodeToElevate).getCoordinates());
            break;
          }
        }
      }
    }
  }

  if (!nodesToDemolish.empty())
  {
    demolishNode(nodesToDemolish);
  }

  for (Point node : nodesToBeUpdated)
  {
    getMapNode(node).setAutotileBitMask(calculateAutotileBitmask(node));
  }

  for (Point node : nodesToBeUpdated)
  {
    getMapNode(node).updateTexture();
  }
}

void MapFunctions::updateAllNodes()
{
  std::vector<Point> allCoords(m_map->mapNodes.size());
  std::transform(m_map->mapNodes.begin(), m_map->mapNodes.end(), allCoords.begin(),
                 [](MapNode &mn) { return mn.getCoordinates(); });
  updateNodeNeighbors(allCoords);
  refreshVisibleMap();
}

std::vector<NeighborNode> MapFunctions::getNeighborNodes(const Point &isoCoordinates, const bool includeCentralNode)
{
  std::vector<NeighborNode> neighbors;
  // ⚡ Bolt: Reserve capacity based on maximum possible neighbors (distance=1 means 9 nodes max)
  neighbors.reserve(includeCentralNode ? 9 : 8);

  for (auto it : PointFunctions::getNeighbors(isoCoordinates, includeCentralNode))
  {
    neighbors.push_back({&getMapNode(it), PointFunctions::getNeighborPositionToOrigin(it, isoCoordinates)});
  }

  return neighbors;
}

bool MapFunctions::isPlacementOnNodeAllowed(const Point &isoCoordinates, const std::string &tileID) const
{
  return m_map->mapNodes[isoCoordinates.toIndex()].isPlacementAllowed(tileID);
}

bool MapFunctions::isPlacementOnAreaAllowed(const std::vector<Point> &targetCoordinates, const std::string &tileID) const
{
  // This function can be divided into two policies:
  // Whether we need all nodes in the area to be placed or not
  bool shouldAllNodesPlaced = true;
  bool areaPlacementAllowed = true;
  bool tilePlacementAllowed = true;

  const Layer layer = TileManager::instance().getTileLayer(tileID);
  // Only buildings and roads has to be placed on all of the tile selected.
  // Other layers can be placed on part of tile in the area, such as zone, water, flora.
  if (layer == BUILDINGS)
  {
    shouldAllNodesPlaced = true;
  }
  else
  {
    shouldAllNodesPlaced = false;
  }
  areaPlacementAllowed = shouldAllNodesPlaced;

  for (auto coord : targetCoordinates)
  {
    tilePlacementAllowed = isPlacementOnNodeAllowed(coord, tileID);

    if (tilePlacementAllowed && !shouldAllNodesPlaced)
    {
      areaPlacementAllowed = true;
      break;
    }
    if (!tilePlacementAllowed && shouldAllNodesPlaced)
    {
      areaPlacementAllowed = false;
      break;
    }
  }

  return areaPlacementAllowed;
}

unsigned char MapFunctions::getElevatedNeighborBitmask(Point centerCoordinates)
{
  unsigned char bitmask = 0;
  const auto centralNodeHeight = getMapNode(centerCoordinates).getCoordinates().height;

  for (const auto &neighborCoordinates : PointFunctions::getNeighbors(centerCoordinates, false))
  {
    if (getMapNode(neighborCoordinates).getCoordinates().height > centralNodeHeight)
    {
      bitmask |= PointFunctions::getNeighborPositionToOrigin(neighborCoordinates, centerCoordinates);
    }
  }

  return bitmask;
}

Point MapFunctions::getNodeOrigCornerPoint(const Point &isoCoordinates, Layer layer)
{
  if ((layer != Layer::NONE) && isoCoordinates.isWithinMapBoundaries())
  {
    return getMapNode(isoCoordinates).getOrigCornerPoint(layer);
  }

  return Point::INVALID();
}

std::vector<uint8_t> MapFunctions::calculateAutotileBitmask(Point coordinate)
{
  std::vector<uint8_t> tileOrientationBitmask(LAYERS_COUNT, 0);

  // ⚡ Bolt: Cache neighbor nodes once instead of querying multiple times per layer
  const std::vector<NeighborNode> neighbors = getNeighborNodes(coordinate, false);

  for (auto currentLayer : allLayersOrdered)
  {
    auto pCurrentTileData = getMapNode(coordinate).getMapNodeDataForLayer(currentLayer).tileData;

    if (pCurrentTileData)
    {
      if (pCurrentTileData->tileType == +TileType::TERRAIN)
      {
        for (const auto &neighbor : neighbors)
        {
          const auto pTileData = neighbor.pNode->getMapNodeDataForLayer(Layer::WATER).tileData;

          if (pTileData && pTileData->tileType == +TileType::WATER)
          {
            tileOrientationBitmask[currentLayer] |= neighbor.position;
          }
        }
      }

      // only auto-tile categories that can be tiled.
      const std::string &nodeTileId = getMapNode(coordinate).getMapNodeDataForLayer(currentLayer).tileID;
      if (TileManager::instance().isTileIDAutoTile(nodeTileId))
      {
        for (const auto &neighbor : neighbors)
        {
          const MapNodeData &nodeData = neighbor.pNode->getMapNodeDataForLayer(currentLayer);

          if (nodeData.tileData && ((nodeData.tileID == nodeTileId) || (pCurrentTileData->tileType == +TileType::ROAD)))
          {
            tileOrientationBitmask[currentLayer] |= neighbor.position;
          }
        }
      }
    }
  }
  return tileOrientationBitmask;
}

bool MapFunctions::setTileID(const std::string &tileID, Point coordinate)
{
  TileData *tileData = TileManager::instance().getTileData(tileID);
  std::vector<Point> targetCoordinates = TileManager::instance().getTargetCoordsOfTileID(coordinate, tileID);

  if (!tileData || targetCoordinates.empty())
  { // if the node would not outside of map boundaries, targetCoordinates would be empty
    return false;
  }

  if (!isPlacementOnAreaAllowed(targetCoordinates, tileID))
    return false;

  Layer layer = TileManager::instance().getTileLayer(tileID);
  std::string randomGroundDecorationTileID;
  std::vector<Point> nodesToBeUpdated;

  // if this building has groundDeco, grab a random tileID from the list
  if (!tileData->groundDecoration.empty())
  {
    randomGroundDecorationTileID =
        *Randomizer::instance().choose(tileData->groundDecoration.begin(), tileData->groundDecoration.end());
  }

  // for >1x1 buildings, clear all the nodes that are going to be occupied before placing anything.
  if (targetCoordinates.size() >= 1 && layer == +Layer::BUILDINGS)
  {
    demolishNode(targetCoordinates, false, Layer::FLORA);      // remove trees under the buildings
    demolishNode(targetCoordinates, false, Layer::POWERLINES); // remove power lines under buildings
  }

  for (auto coord : targetCoordinates)
  { // now we can place our building

    MapNode &currentMapNode = getMapNode(coord);

    if (coord != coordinate && targetCoordinates.size() > 1)
    { // for buildings >1x1 set every node on the layer that will be occupied to invisible except of the origin node
      currentMapNode.getSprite()->setRenderFlag(layer, false);
    }
    else
    { // 1x1 buildings should be set to visible
      currentMapNode.getSprite()->setRenderFlag(layer, true);
    }

    if ((!targetCoordinates.size()) == 1)
    { // if it's not a >1x1 building, place tileID on the current coordinate (e.g. ground decoration beneath a > 1x1 building)
      currentMapNode.setTileID(tileID, coord);
    }
    else
    { // set the tileID for the mapNode of the origin coordinates only on the origin coordinate
      currentMapNode.setTileID(tileID, coordinate);
    }

    // place ground deco if we have one
    if (!randomGroundDecorationTileID.empty())
    {
      currentMapNode.setTileID(randomGroundDecorationTileID, coord);
    }

    // For layers that autotile to each other, we need to update their neighbors too
    if (TileManager::instance().isTileIDAutoTile(tileID))
    {
      nodesToBeUpdated.push_back(currentMapNode.getCoordinates());
    }

    // emit a signal that setTileID has been called
    SignalMediator::instance().signalSetTileID.emit(currentMapNode);
  }

  if (!nodesToBeUpdated.empty())
  {
    updateNodeNeighbors(nodesToBeUpdated);
  }
  return true;
}

bool MapFunctions::setTileID(const std::string &tileID, const std::vector<Point> &coordinates)
{
  bool setTileResult = false;
  for (auto coord : coordinates)
  {
    setTileResult |= setTileID(tileID, coord);
  }
  return setTileResult;
}

void MapFunctions::demolishNode(const std::vector<Point> &isoCoordinates, bool updateNeighboringTiles, Layer layer)
{
  std::vector<Point> nodesToDemolish;

  for (Point currentCoordinate : isoCoordinates)
  {
    if (currentCoordinate.isWithinMapBoundaries())
    {
      const MapNode &node = getMapNode(currentCoordinate);

      // Check for multi-node buildings first. Those are on the buildings layer, even if we want to demolish another layer than Buildings.
      // In case we add more Layers that support Multi-node, add a for loop here
      // If demolishNode is called for layer GROUNDECORATION, we'll still need to gather all nodes from the multi-node building to delete the decoration under the entire building
      auto pNodeTileData = node.getMapNodeDataForLayer(Layer::BUILDINGS).tileData;

      if (pNodeTileData && ((pNodeTileData->RequiredTiles.height > 1) || (pNodeTileData->RequiredTiles.width > 1)))
      {
        const Point origCornerPoint = node.getOrigCornerPoint(Layer::BUILDINGS);

        if (origCornerPoint.isWithinMapBoundaries())
        {
          const std::string &tileID = getMapNode(origCornerPoint).getTileID(Layer::BUILDINGS);

          // get all the occupied nodes and demolish them
          for (auto buildingCoords : TileManager::instance().getTargetCoordsOfTileID(origCornerPoint, tileID))
          {
            nodesToDemolish.push_back(buildingCoords);
          }
        }
      }
      // add the current coorindate
      nodesToDemolish.push_back(currentCoordinate);
    }
  }

  std::vector<Point> updateNodes;
  for (auto nodeCoordinate : nodesToDemolish)
  {
    getMapNode(nodeCoordinate).demolishNode(layer);
    SignalMediator::instance().signalDemolish.emit(&getMapNode(nodeCoordinate));
    // TODO: Play sound effect here
    if (updateNeighboringTiles)
    {
      updateNodes.push_back(nodeCoordinate);
    }
  }

  if (!updateNodes.empty())
  {
    updateNodeNeighbors(updateNodes);
  }
}

void MapFunctions::getNodeInformation(const Point &isoCoordinates) const
{
  const MapNode &mapNode = m_map->mapNodes[isoCoordinates.toIndex()];
  const MapNodeData &mapNodeData = mapNode.getActiveMapNodeData();
  const TileData *tileData = mapNodeData.tileData;
  LOG(LOG_INFO) << "===== TILE at " << isoCoordinates.x << ", " << isoCoordinates.y << ", " << mapNode.getCoordinates().height
                << "=====";
  LOG(LOG_INFO) << "[Layer: TERRAIN] ID: " << mapNode.getMapNodeDataForLayer(Layer::TERRAIN).tileID;
  LOG(LOG_INFO) << "[Layer: WATER] ID: " << mapNode.getMapNodeDataForLayer(Layer::WATER).tileID;
  LOG(LOG_INFO) << "[Layer: BUILDINGS] ID: " << mapNode.getMapNodeDataForLayer(Layer::BUILDINGS).tileID;
  LOG(LOG_INFO) << "Category: " << tileData->category;
  LOG(LOG_INFO) << "FileName: " << tileData->tiles.fileName;
  LOG(LOG_INFO) << "PickRandomTile: " << tileData->tiles.pickRandomTile;
  LOG(LOG_INFO) << "TileMap: " << mapNodeData.tileMap;
  LOG(LOG_INFO) << "TileIndex: " << mapNodeData.tileIndex;
}

void MapFunctions::highlightNode(const Point &isoCoordinates, const SpriteRGBColor &rgbColor)
{
  if (isoCoordinates.isWithinMapBoundaries())
  {
    const auto pSprite = getMapNode(isoCoordinates).getSprite();
    pSprite->highlightColor = rgbColor;
    pSprite->highlightSprite = true;
  }
}

std::string MapFunctions::getTileID(const Point &isoCoordinates, Layer layer)
{
  return (isoCoordinates.isWithinMapBoundaries()) ? getMapNode(isoCoordinates).getTileID(layer) : "";
}

void MapFunctions::unHighlightNode(const Point &isoCoordinates)
{
  if (isoCoordinates.isWithinMapBoundaries())
  {
    getMapNode(isoCoordinates).getSprite()->highlightSprite = false;
  }
}

Point MapFunctions::findNodeInMap(const SDL_Point &screenCoordinates, const Layer &layer)
{
  // calculate clicked column (x coordinate) without height taken into account.
  const Point calculatedIsoCoords = calculateIsoCoordinates(screenCoordinates);
  int isoX = calculatedIsoCoords.x;
  int isoY = calculatedIsoCoords.y;

  // adjust calculated values that are outside of the map (which is legit, but they need to get pushed down)
  // only y can be out of bounds on our map
  if (isoY >= Settings::instance().mapSize)
  {
    int diff = isoY - Settings::instance().mapSize; // the diff to reset the value to the edge of the map
    // travel the column downwards.
    isoX += diff;
    isoY -= diff;
  }

  // Transverse a column form from calculated coordinates to the bottom of the map.
  // It is necessary to include 2 neighbor nodes from both sides.
  // Try to find map node in Z order.
  // Node with the highest Z order has highest X and lowest Y coordinate, so search will be conducted in that order.
  const int neighborReach = 2;
  const int mapSize = Settings::instance().mapSize;

  // Max X will reach end of the map or Y will reach 0.
  const int xMax = std::min(isoX + neighborReach + isoY, mapSize - 1);
  // Min X will reach 0 or x -2 neighbor node.
  const int xMin = std::max(isoX - neighborReach, 0);

  for (int x = xMax; x >= xMin; --x)
  {
    const int diff = x - isoX;
    const int yMiddlePoint = isoY - diff;

    // Move y up and down 2 neighbors.
    for (int y = std::max(yMiddlePoint - neighborReach, 0); (y <= yMiddlePoint + neighborReach) && (y < mapSize); ++y)
    {
      //get all coordinates for node at x,y
      Point coordinate = getMapNode(Point(x, y)).getCoordinates();

      if (isClickWithinTile(screenCoordinates, coordinate, layer))
      {
        return coordinate;
      }
    }
  }

  return Point{-1, -1, 0, 0};
}

bool MapFunctions::isClickWithinTile(const SDL_Point &screenCoordinates, Point isoCoordinate,
                                     const Layer &layer = Layer::NONE) const
{
  if (!isoCoordinate.isWithinMapBoundaries())
  {
    return false;
  }

  const auto &node = m_map->mapNodes[isoCoordinate.toIndex()];
  auto pSprite = node.getSprite();

  // Layers ordered for hitcheck — fixed-size array avoids heap allocation.
  static constexpr Layer defaultLayers[] = {Layer::TERRAIN, Layer::WATER, Layer::UNDERGROUND, Layer::BLUEPRINT};

  const Layer *layersBegin = nullptr;
  const Layer *layersEnd   = nullptr;

  if (layer == Layer::NONE)
  {
    layersBegin = std::begin(defaultLayers);
    layersEnd   = std::end(defaultLayers);
  }
  else
  {
    layersBegin = &layer;
    layersEnd   = &layer + 1;
  }

  for (const Layer *it = layersBegin; it != layersEnd; ++it)
  {
    const Layer curLayer = *it;
    if (!MapLayers::isLayerActive(curLayer))
    {
      continue;
    }

    SDL_Rect spriteRect = pSprite->getDestRect(curLayer);
    SDL_Rect clipRect = pSprite->getClipRect(curLayer);

    if (curLayer == Layer::TERRAIN)
    {
      clipRect.h += 1; //HACK: We need to increase clipRect height by one pixel to match the drawRect. Rounding issue?
    }

    if (SDL_PointInRect(&screenCoordinates, &spriteRect))
    {
      std::string tileID = node.getMapNodeDataForLayer(curLayer).tileID;
      assert(!tileID.empty());

      // Calculate the position of the clicked pixel within the surface and "un-zoom" the position to match the un-adjusted surface
      const int pixelX = static_cast<int>((screenCoordinates.x - spriteRect.x) / Camera::instance().zoomLevel()) + clipRect.x;
      const int pixelY = static_cast<int>((screenCoordinates.y - spriteRect.y) / Camera::instance().zoomLevel()) + clipRect.y;

      if ((curLayer == Layer::TERRAIN) && (node.getMapNodeDataForLayer(Layer::TERRAIN).tileMap == TileMap::SHORE))
      {
        tileID.append("_shore");
      }

      // Check if the clicked Sprite is not transparent (we hit a point within the pixel)
      if (ResourcesManager::instance().getColorOfPixelInSurface(tileID, pixelX, pixelY).a != SDL_ALPHA_TRANSPARENT)
      {
        return true;
      }
    }
  }

  // Nothing found
  return false;
}
void MapFunctions::newMap(bool generateTerrain)
{
  const int mapSize = Settings::instance().mapSize;
  m_map = std::make_unique<Map>(mapSize, mapSize, generateTerrain);
  updateAllNodes();
}

void MapFunctions::loadMapFromFile(const std::string &fileName)
{
  std::string jsonAsString = fs::readCompressedFileAsString(CYTOPIA_SAVEGAME_DIR + fileName);

  if (jsonAsString.empty())
  {
    throw ConfigurationError(TRACE_INFO "Failed to load savegame file " + fileName);
  }

  json saveGameJSON = json::parse(jsonAsString, nullptr, false);

  if (saveGameJSON.is_discarded())
    throw ConfigurationError(TRACE_INFO "Could not parse savegame file " + fileName);

  size_t saveGameVersion = saveGameJSON.value("Savegame version", 0);
  if (saveGameVersion < 4 || saveGameVersion > SAVEGAME_VERSION)
  {
    throw CytopiaError(TRACE_INFO "Trying to load a Savegame with version " + std::to_string(saveGameVersion) +
                       " but only save-games with version range [4, " + std::to_string(SAVEGAME_VERSION) + "] are supported");
  }

  int columns = saveGameJSON.value("columns", -1);
  int rows = saveGameJSON.value("rows", -1);

  if (columns == -1 || rows == -1)
  {
    throw ConfigurationError(TRACE_INFO "Savegame file " + fileName + "is invalid!");
  }

  auto map = std::make_unique<Map>(columns, rows);
  map->mapNodes.reserve(columns * rows);

  for (const auto &it : saveGameJSON["mapNode"].items())
  {
    Point coordinates = json(it.value())["coordinates"].get<Point>();
    map->mapNodes.emplace_back(Point{coordinates.x, coordinates.y, coordinates.z, coordinates.height}, "");
    map->mapNodes.back().setMapNodeData(json(it.value())["mapNodeData"]);
  }

  for (auto &node : map->mapNodes)
  {
    for (const auto &tile : node.getMapNodeData())
    {
      node.setTileID(tile.tileID, tile.origCornerPoint);
    }
  }

  for (int x = 0; x < columns; x++)
  {
    for (int y = columns - 1; y >= 0; y--)
    {
      map->mapNodesInDrawingOrder.emplace_back(&map->mapNodes[x * columns + y]);
    }
  }

  m_map = std::move(map);
  updateAllNodes();

  if (saveGameJSON.contains("simulation") && saveGameJSON["simulation"].is_object())
  {
    const json &simulation = saveGameJSON["simulation"];

    if (simulation.contains("affordability") && simulation["affordability"].is_object())
    {
      const json &aff = simulation["affordability"];
      AffordabilityState &state = AffordabilityModel::instance().mutableState();
      state.medianRent = aff.value("medianRent", state.medianRent);
      state.medianIncome = aff.value("medianIncome", state.medianIncome);
      state.affordabilityIndex = aff.value("affordabilityIndex", state.affordabilityIndex);
      state.displacementPressure = aff.value("displacementPressure", state.displacementPressure);
      state.landValueProxy = aff.value("landValueProxy", state.landValueProxy);
    }

    if (simulation.contains("governance") && simulation["governance"].is_object())
    {
      const json &gov = simulation["governance"];
      GovernancePersistedState state = GovernanceSystem::instance().persistedState();
      state.approval = gov.value("approval", state.approval);
      state.totalMonthsElapsed = gov.value("totalMonthsElapsed", state.totalMonthsElapsed);
      state.monthsSinceCheckpoint = gov.value("monthsSinceCheckpoint", state.monthsSinceCheckpoint);
      state.policyLockMonthsRemaining = gov.value("policyLockMonthsRemaining", state.policyLockMonthsRemaining);
      state.policyConstrained = gov.value("policyConstrained", state.policyConstrained);
      state.lostElection = gov.value("lostElection", state.lostElection);
      state.checkpointPending = gov.value("checkpointPending", state.checkpointPending);
      state.taxEfficiencyMultiplier = gov.value("taxEfficiencyMultiplier", state.taxEfficiencyMultiplier);
      state.incomeModifier = gov.value("incomeModifier", state.incomeModifier);
      GovernanceSystem::instance().applyPersistedState(state);
    }

    if (simulation.contains("budget") && simulation["budget"].is_object())
    {
      const json &budgetJson = simulation["budget"];
      BudgetPersistedState budget = BudgetSystem::instance().persistedState();
      budget.runningBalance = budgetJson.value("runningBalance", budget.runningBalance);
      budget.lastRevenue = budgetJson.value("lastRevenue", budget.lastRevenue);
      budget.lastExpenses = budgetJson.value("lastExpenses", budget.lastExpenses);
      budget.month = budgetJson.value("month", budget.month);
      BudgetSystem::instance().applyPersistedState(budget);
    }

    if (simulation.contains("economyDepth") && simulation["economyDepth"].is_object())
    {
      const json &eco = simulation["economyDepth"];
      EconomyDepthState &state = EconomyDepthModel::instance().mutableState();
      state.unemploymentPressure = eco.value("unemploymentPressure", state.unemploymentPressure);
      state.wagePressure = eco.value("wagePressure", state.wagePressure);
      state.businessConfidence = eco.value("businessConfidence", state.businessConfidence);
      state.debtStress = eco.value("debtStress", state.debtStress);
    }

    if (simulation.contains("serviceStrain") && simulation["serviceStrain"].is_object())
    {
      const json &services = simulation["serviceStrain"];
      ServiceStrainState &state = ServiceStrainModel::instance().mutableState();
      state.transitReliability = services.value("transitReliability", state.transitReliability);
      state.safetyCapacityLoad = services.value("safetyCapacityLoad", state.safetyCapacityLoad);
      state.educationAccessStress = services.value("educationAccessStress", state.educationAccessStress);
      state.healthAccessStress = services.value("healthAccessStress", state.healthAccessStress);
    }

    if (simulation.contains("context") && simulation["context"].is_object())
    {
      const json &ctxJson = simulation["context"];
      SimulationContextData &ctx = SimulationContext::instance().mutableData();
      ctx.month = ctxJson.value("month", ctx.month);
      ctx.taxEfficiency = ctxJson.value("taxEfficiency", ctx.taxEfficiency);
      ctx.approvalMultiplier = ctxJson.value("approvalMultiplier", ctx.approvalMultiplier);
      ctx.growthRateModifier = ctxJson.value("growthRateModifier", ctx.growthRateModifier);
      ctx.unemploymentPressure = ctxJson.value("unemploymentPressure", ctx.unemploymentPressure);
      ctx.wagePressure = ctxJson.value("wagePressure", ctx.wagePressure);
      ctx.businessConfidence = ctxJson.value("businessConfidence", ctx.businessConfidence);
      ctx.debtStress = ctxJson.value("debtStress", ctx.debtStress);
      ctx.transitReliability = ctxJson.value("transitReliability", ctx.transitReliability);
      ctx.safetyCapacityLoad = ctxJson.value("safetyCapacityLoad", ctx.safetyCapacityLoad);
      ctx.educationAccessStress = ctxJson.value("educationAccessStress", ctx.educationAccessStress);
      ctx.healthAccessStress = ctxJson.value("healthAccessStress", ctx.healthAccessStress);
    }

    for (const auto &definition : PolicyEngine::instance().definitions())
      PolicyEngine::instance().setPolicyLevel(definition.id, 0);

    if (simulation.contains("policy_levels") && simulation["policy_levels"].is_array())
    {
      for (const auto &entry : simulation["policy_levels"])
      {
        if (!entry.is_object())
          continue;
        const std::string policyId = entry.value("id", std::string{});
        const int level = entry.value("level", 0);
        if (!policyId.empty())
          PolicyEngine::instance().setPolicyLevel(policyId, level);
      }
    }
    else if (simulation.contains("activePolicies") && simulation["activePolicies"].is_array())
    {
      for (const auto &entry : simulation["activePolicies"])
      {
        if (entry.is_string())
          PolicyEngine::instance().setActive(entry.get<std::string>(), true);
      }
    }
  }

  LOG(LOG_DEBUG) << "Load succesfully: " << CYTOPIA_SAVEGAME_DIR + fileName;
}

void MapFunctions::saveMapToFile(const std::string &fileName)
{
  // make sure savegame dir exists
  fs::createDirectory(CYTOPIA_SAVEGAME_DIR);

  const AffordabilityState &affordability = AffordabilityModel::instance().state();
  const GovernancePersistedState governance = GovernanceSystem::instance().persistedState();
  const BudgetPersistedState budget = BudgetSystem::instance().persistedState();
  const EconomyDepthState &economy = EconomyDepthModel::instance().state();
  const ServiceStrainState &services = ServiceStrainModel::instance().state();
  const SimulationContextData &context = SimulationContext::instance().data();

  json policyLevels = json::array();
  json activePolicies = json::array();
  for (const auto &definition : PolicyEngine::instance().definitions())
  {
    const int level = PolicyEngine::instance().policyLevel(definition.id);
    if (level > 0)
    {
      policyLevels.push_back({{"id", definition.id}, {"level", level}});
      activePolicies.push_back(definition.id);
    }
  }

  const json simulation = json{
      {"affordability",
       {{"medianRent", affordability.medianRent},
        {"medianIncome", affordability.medianIncome},
        {"affordabilityIndex", affordability.affordabilityIndex},
        {"displacementPressure", affordability.displacementPressure},
        {"landValueProxy", affordability.landValueProxy}}},
      {"governance",
       {{"approval", governance.approval},
        {"totalMonthsElapsed", governance.totalMonthsElapsed},
        {"monthsSinceCheckpoint", governance.monthsSinceCheckpoint},
        {"policyLockMonthsRemaining", governance.policyLockMonthsRemaining},
        {"policyConstrained", governance.policyConstrained},
        {"lostElection", governance.lostElection},
        {"checkpointPending", governance.checkpointPending},
        {"taxEfficiencyMultiplier", governance.taxEfficiencyMultiplier},
        {"incomeModifier", governance.incomeModifier}}},
      {"budget",
       {{"runningBalance", budget.runningBalance},
        {"lastRevenue", budget.lastRevenue},
        {"lastExpenses", budget.lastExpenses},
        {"month", budget.month}}},
      {"economyDepth",
       {{"unemploymentPressure", economy.unemploymentPressure},
        {"wagePressure", economy.wagePressure},
        {"businessConfidence", economy.businessConfidence},
        {"debtStress", economy.debtStress}}},
      {"serviceStrain",
       {{"transitReliability", services.transitReliability},
        {"safetyCapacityLoad", services.safetyCapacityLoad},
        {"educationAccessStress", services.educationAccessStress},
        {"healthAccessStress", services.healthAccessStress}}},
      {"context",
       {{"month", context.month},
        {"taxEfficiency", context.taxEfficiency},
        {"approvalMultiplier", context.approvalMultiplier},
        {"growthRateModifier", context.growthRateModifier},
        {"unemploymentPressure", context.unemploymentPressure},
        {"wagePressure", context.wagePressure},
        {"businessConfidence", context.businessConfidence},
        {"debtStress", context.debtStress},
        {"transitReliability", context.transitReliability},
        {"safetyCapacityLoad", context.safetyCapacityLoad},
        {"educationAccessStress", context.educationAccessStress},
        {"healthAccessStress", context.healthAccessStress}}},
      {"policy_levels", policyLevels},
      {"activePolicies", activePolicies}};

  //create savegame json string
  const json j = json{{"Savegame version", SAVEGAME_VERSION},
                      {"columns", m_map->m_columns},
                      {"rows", m_map->m_rows},
                      {"mapNode", m_map->mapNodes},
                      {"simulation", simulation}};

#ifdef DEBUG
  // Write uncompressed savegame for easier debugging
  fs::writeStringToFile(CYTOPIA_SAVEGAME_DIR + fileName + ".txt", j.dump());
#endif
  fs::writeStringToFileCompressed(CYTOPIA_SAVEGAME_DIR + fileName, j.dump());
  LOG(LOG_DEBUG) << "Saved succesfully: " << CYTOPIA_SAVEGAME_DIR + fileName;
}

void MapFunctions::refreshVisibleMap()
{
  if (m_map)
  {
    m_map->refresh();
  }
}
