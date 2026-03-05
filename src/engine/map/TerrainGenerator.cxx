#include "TerrainGenerator.hxx"

#include "Constants.hxx"
#include "LOG.hxx"
#include "Exception.hxx"
#include "JsonSerialization.hxx"
#include "Filesystem.hxx"
#include <PointFunctions.hxx>

#include "json.hxx"

#include <noise/noise.h>

#include <random>

using json = nlohmann::json;

namespace
{
// Noise module tile scale factor used to convert grid coordinates to noise-space
constexpr double NOISE_TILE_SCALE = 32.0;

// Terrain height Perlin noise parameters
constexpr double TERRAIN_HEIGHT_PERLIN_FREQUENCY = 0.003 / NOISE_TILE_SCALE;
constexpr double TERRAIN_HEIGHT_PERLIN_LACUNARITY = 1.5;
constexpr int TERRAIN_HEIGHT_PERLIN_OCTAVES = 16;
constexpr double TERRAIN_HEIGHT_PERLIN_SCALE = 0.25;
constexpr double TERRAIN_HEIGHT_PERLIN_BIAS = -0.5;

// Terrain height fractal (ridged multi) noise parameters
constexpr double TERRAIN_HEIGHT_FRACTAL_FREQUENCY = 0.005 / NOISE_TILE_SCALE;
constexpr double TERRAIN_HEIGHT_FRACTAL_LACUNARITY = 2.0;
constexpr double TERRAIN_HEIGHT_FRACTAL_SCALE_FACTOR = 0.025; ///< Multiplied by mountainAmplitude
constexpr double TERRAIN_HEIGHT_FRACTAL_BIAS = 0.5;

// Blend control noise parameters
constexpr int TERRAIN_HEIGHT_BLEND_SEED_OFFSET = 1;
constexpr double TERRAIN_HEIGHT_BLEND_FREQUENCY = 0.005 / NOISE_TILE_SCALE;
constexpr double TERRAIN_HEIGHT_BLEND_SCALE = 2.0;
constexpr double TERRAIN_HEIGHT_BLEND_BIAS_FACTOR = 0.1; ///< Multiplied by mountainAmplitude

// Final terrain height scale parameters
constexpr double TERRAIN_HEIGHT_OUTPUT_SCALE = 20.0;
constexpr double TERRAIN_HEIGHT_OUTPUT_BIAS = 4.0;
constexpr double TERRAIN_HEIGHT_CLAMP_MIN = 0.0;
constexpr double TERRAIN_HEIGHT_CLAMP_MAX = 255.0;

// Foliage density noise parameters
constexpr int FOLIAGE_DENSITY_SEED_OFFSET = 1234;
constexpr double FOLIAGE_DENSITY_FREQUENCY = 0.05 / NOISE_TILE_SCALE;

// High-frequency arbitrary noise parameters
constexpr int HIGH_FREQ_NOISE_SEED_OFFSET = 42;
constexpr double HIGH_FREQ_NOISE_FREQUENCY = 1.0;

// Foliage density thresholds
constexpr double FOLIAGE_DENSITY_LIGHT_MAX = 0.1;
constexpr double FOLIAGE_DENSITY_MEDIUM_MAX = 0.25;
constexpr double FOLIAGE_DENSITY_DENSE_MAX = 1.0;

// Foliage tile selection thresholds (maximum tileIndex to accept)
constexpr int FOLIAGE_LIGHT_TILE_THRESHOLD = 20;
constexpr int FOLIAGE_MEDIUM_TILE_THRESHOLD = 50;
constexpr int FOLIAGE_DENSE_TILE_THRESHOLD = 95;

// Scaling factor for high-frequency noise used in tile index selection
constexpr double FOLIAGE_TILE_INDEX_NOISE_SCALE = 200.0;
} // namespace

void TerrainGenerator::generateTerrain(std::vector<MapNode> &mapNodes, std::vector<MapNode *> &mapNodesInDrawingOrder)
{
  loadTerrainDataFromJSON();

  if (m_terrainSettings.seed == 0)
  {
    srand(static_cast<unsigned int>(time(nullptr)));
    m_terrainSettings.seed = rand();
  }

  noise::module::Perlin terrainHeightPerlin;
  terrainHeightPerlin.SetSeed(m_terrainSettings.seed);
  terrainHeightPerlin.SetFrequency(TERRAIN_HEIGHT_PERLIN_FREQUENCY);
  terrainHeightPerlin.SetLacunarity(TERRAIN_HEIGHT_PERLIN_LACUNARITY);
  terrainHeightPerlin.SetOctaveCount(TERRAIN_HEIGHT_PERLIN_OCTAVES);
  noise::module::ScaleBias terrainHeightPerlinScaled;
  terrainHeightPerlinScaled.SetSourceModule(0, terrainHeightPerlin);
  terrainHeightPerlinScaled.SetScale(TERRAIN_HEIGHT_PERLIN_SCALE);
  terrainHeightPerlinScaled.SetBias(TERRAIN_HEIGHT_PERLIN_BIAS);

  noise::module::RidgedMulti terrainHeightFractal;
  terrainHeightFractal.SetSeed(m_terrainSettings.seed);
  terrainHeightFractal.SetFrequency(TERRAIN_HEIGHT_FRACTAL_FREQUENCY);
  terrainHeightFractal.SetLacunarity(TERRAIN_HEIGHT_FRACTAL_LACUNARITY);
  noise::module::ScaleBias terrainHeightFractalScaled;
  terrainHeightFractalScaled.SetSourceModule(0, terrainHeightFractal);
  terrainHeightFractalScaled.SetScale(m_terrainSettings.mountainAmplitude * TERRAIN_HEIGHT_FRACTAL_SCALE_FACTOR);
  terrainHeightFractalScaled.SetBias(TERRAIN_HEIGHT_FRACTAL_BIAS);

  noise::module::Perlin terrainHeightBlendPerlin;
  terrainHeightBlendPerlin.SetSeed(m_terrainSettings.seed + TERRAIN_HEIGHT_BLEND_SEED_OFFSET);
  terrainHeightBlendPerlin.SetFrequency(TERRAIN_HEIGHT_BLEND_FREQUENCY);
  noise::module::ScaleBias terrainHeightBlendScale;
  terrainHeightBlendScale.SetSourceModule(0, terrainHeightBlendPerlin);
  terrainHeightBlendScale.SetScale(TERRAIN_HEIGHT_BLEND_SCALE);
  terrainHeightBlendScale.SetBias(-TERRAIN_HEIGHT_BLEND_BIAS_FACTOR * m_terrainSettings.mountainAmplitude);
  noise::module::Clamp terrainHeightBlendControl;
  terrainHeightBlendControl.SetSourceModule(0, terrainHeightBlendScale);
  terrainHeightBlendControl.SetBounds(0, 1);

  noise::module::Blend terrainHeightBlend;
  terrainHeightBlend.SetSourceModule(0, terrainHeightPerlinScaled);
  terrainHeightBlend.SetSourceModule(1, terrainHeightFractalScaled);
  terrainHeightBlend.SetControlModule(terrainHeightBlendControl);

  noise::module::ScaleBias terrainHeightScale;
  terrainHeightScale.SetSourceModule(0, terrainHeightBlend);
  terrainHeightScale.SetScale(TERRAIN_HEIGHT_OUTPUT_SCALE);
  terrainHeightScale.SetBias(TERRAIN_HEIGHT_OUTPUT_BIAS);

  noise::module::Clamp terrainHeight;
  terrainHeight.SetSourceModule(0, terrainHeightScale);
  terrainHeight.SetBounds(TERRAIN_HEIGHT_CLAMP_MIN, TERRAIN_HEIGHT_CLAMP_MAX);

  // Foliage
  noise::module::Perlin foliageDensityPerlin;
  foliageDensityPerlin.SetSeed(m_terrainSettings.seed + FOLIAGE_DENSITY_SEED_OFFSET);
  foliageDensityPerlin.SetFrequency(FOLIAGE_DENSITY_FREQUENCY);

  // Arbitrary Noise
  noise::module::Perlin highFrequencyNoise;
  highFrequencyNoise.SetSeed(m_terrainSettings.seed + HIGH_FREQ_NOISE_SEED_OFFSET);
  highFrequencyNoise.SetFrequency(HIGH_FREQ_NOISE_FREQUENCY);

  const int mapSize = m_terrainSettings.mapSize;
  const size_t vectorSize = static_cast<size_t>(mapSize * mapSize);
  mapNodes.reserve(vectorSize);

  // River random source
  std::minstd_rand riverRand;
  riverRand.seed(m_terrainSettings.seed);

  // For now, the biome string is read from settings.json for debugging
  std::string currentBiome = Settings::instance().biome;

  // nodes need to be created at the correct vector "coordinates", or else the Z-Order will be broken
  for (int x = 0; x < mapSize; x++)
  {
    for (int y = 0; y < mapSize; y++)
    {
      const int z = 0; // it's not possible to calculate the correct z-index, so set it later in a for loop
      double rawHeight = terrainHeight.GetValue(x * NOISE_TILE_SCALE, y * NOISE_TILE_SCALE, 0.5);
      int height = static_cast<int>(rawHeight);

      if (height < m_terrainSettings.seaLevel)
      {
        height = m_terrainSettings.seaLevel;
        mapNodes.emplace_back(MapNode{Point{x, y, z, height, rawHeight}, m_biomeInformation[currentBiome].water[0]});
      }
      else
      {
        const double foliageDensity =
            foliageDensityPerlin.GetValue(x * NOISE_TILE_SCALE, y * NOISE_TILE_SCALE, height / NOISE_TILE_SCALE);
        bool placed = false;

        if (foliageDensity > 0.0 && height > m_terrainSettings.seaLevel)
        {
          int tileIndex = static_cast<int>(std::abs(
              round(highFrequencyNoise.GetValue(x * NOISE_TILE_SCALE, y * NOISE_TILE_SCALE, height / NOISE_TILE_SCALE) *
                    FOLIAGE_TILE_INDEX_NOISE_SCALE)));

          if (foliageDensity < FOLIAGE_DENSITY_LIGHT_MAX)
          {
            if (tileIndex < FOLIAGE_LIGHT_TILE_THRESHOLD)
            {
              tileIndex = tileIndex % static_cast<int>(m_biomeInformation[currentBiome].treesLight.size());
              mapNodes.emplace_back(MapNode{Point{x, y, z, height, rawHeight}, m_biomeInformation[currentBiome].terrain[0],
                                            m_biomeInformation[currentBiome].treesLight[tileIndex]});
              placed = true;
            }
          }
          else if (foliageDensity < FOLIAGE_DENSITY_MEDIUM_MAX)
          {
            if (tileIndex < FOLIAGE_MEDIUM_TILE_THRESHOLD)
            {
              tileIndex = tileIndex % static_cast<int>(m_biomeInformation[currentBiome].treesMedium.size());
              mapNodes.emplace_back(MapNode{Point{x, y, z, height, rawHeight}, m_biomeInformation[currentBiome].terrain[0],
                                            m_biomeInformation[currentBiome].treesMedium[tileIndex]});
              placed = true;
            }
          }
          else if (foliageDensity < FOLIAGE_DENSITY_DENSE_MAX && tileIndex < FOLIAGE_DENSE_TILE_THRESHOLD)
          {
            tileIndex = tileIndex % static_cast<int>(m_biomeInformation[currentBiome].treesDense.size());

            mapNodes.emplace_back(MapNode{Point{x, y, z, height, rawHeight}, m_biomeInformation[currentBiome].terrain[0],
                                          m_biomeInformation[currentBiome].treesDense[tileIndex]});
            placed = true;
          }
        }
        if (!placed)
        {
          mapNodes.emplace_back(MapNode{Point{x, y, z, height, rawHeight}, m_biomeInformation[currentBiome].terrain[0]});
        }

        if (height > m_terrainSettings.seaLevel)
        {
          const int riverSourcePossibility = riverRand() % m_terrainSettings.maxHeight;
          if ((riverSourcePossibility >= (m_terrainSettings.maxHeight + m_terrainSettings.seaLevel - height)) && (riverRand() % 50 == 0))
          {
            m_riverSourceNodes.emplace_back(Point{x, y, z, height, rawHeight});
          }
        }
      }
    }
  }

  int z = 0;
  // set the z-Index for the mapNodes. It is not used, but it's better to have the correct z-index set
  for (int y = mapSize - 1; y >= 0; y--)
  {
    for (int x = 0; x < mapSize; x++)
    {
      z++;
      mapNodes[x * mapSize + y].setZIndex(z);
      mapNodesInDrawingOrder.push_back(&mapNodes[x * mapSize + y]);
    }
  }
}

void TerrainGenerator::generateRiver(std::vector<MapNode> &mapNodes)
{
  std::unordered_set<Point> riverNodes;
  std::vector<Point> neighbors;
  Point curPoint, nextPoint;
  double curHeight, nextHeight, neighborHeight;
  int riverNumber = 0;

  for (const auto &riverSourceNode : m_riverSourceNodes)
  {
    curPoint = riverSourceNode;
    curHeight = mapNodes[curPoint.toIndex()].getCoordinates().rawHeight;

    while (true)
    {
      if (riverNodes.find(curPoint) != riverNodes.end())
        riverNodes.insert(curPoint);

      // Find the lowest node
      nextHeight = 32;
      neighbors = PointFunctions::getNeighbors(curPoint, false);
      for (const auto &neighbor : neighbors)
      {
        if (riverNodes.find(neighbor) != riverNodes.end())
          continue;
        neighborHeight = mapNodes[neighbor.toIndex()].getCoordinates().rawHeight;
        if (neighborHeight < nextHeight)
        {
          nextHeight = neighborHeight;
          nextPoint = neighbor;
        }
      }

      // Add lowest nodes in the area to riverNodes
      for (const auto &neighbor : neighbors)
      {
        neighborHeight = mapNodes[neighbor.toIndex()].getCoordinates().rawHeight;
        if (neighborHeight <= nextHeight + 0.005 && (riverNodes.find(neighbor) == riverNodes.end()))
        {
          riverNodes.insert(neighbor);
        }
      }

      // These nodes can reach the sea, so set nodes to river
      if (curHeight < m_terrainSettings.seaLevel)
      {
        for (Point point: riverNodes)
        {
          MapNode *node = &(mapNodes[point.toIndex()]);
          node->setTileID("water", point);
        }
        riverNumber++;
        riverNodes.clear();
        break;
      }

      // Terrain slight rise is not able to stop the river
      if (nextHeight > curHeight + 0.03)
      {
        riverNodes.clear();
        break;
      }

      curPoint = nextPoint;
      curHeight = nextHeight;
    }
  }

  LOG(LOG_INFO) << __func__ << __LINE__ << "Number of rivers: " << riverNumber;
}

void TerrainGenerator::loadTerrainDataFromJSON()
{
  std::string jsonFileContent = fs::readFileAsString(TERRAINGEN_DATA_FILE_NAME);
  json biomeDataJsonObject = json::parse(jsonFileContent, nullptr, false);

  // check if json file can be parsed
  if (biomeDataJsonObject.is_discarded())
    throw ConfigurationError(TRACE_INFO "Error parsing JSON File " + string{TERRAINGEN_DATA_FILE_NAME});

  // parse biome objects
  for (const auto &it : biomeDataJsonObject.items())
  {
    m_biomeInformation[it.key()] = it.value();
  }
}
