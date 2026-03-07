#ifndef SERVICE_STRAIN_MODEL_HXX_
#define SERVICE_STRAIN_MODEL_HXX_

#include "Singleton.hxx"
#include "CityIndices.hxx"
#include "tileData.hxx"

#include <string>
#include <vector>

struct ServiceStrainState
{
  float transitReliability    = 50.f; ///< [0..100], higher = better reliability
  float safetyCapacityLoad    = 50.f; ///< [0..100], higher = more overloaded
  float educationAccessStress = 50.f; ///< [0..100], higher = poorer access
  float healthAccessStress    = 50.f; ///< [0..100], higher = poorer access
};

class ServiceStrainModel : public Singleton<ServiceStrainModel>
{
public:
  friend Singleton<ServiceStrainModel>;

  void reset();

  /**
   * @brief Tick service reliability/strain state once per game-month.
   */
  void tick(const std::vector<const TileData *> &buildingTiles, int roadTileCount, int totalTileCount,
            const CityIndicesData &indices);

  const ServiceStrainState &state() const { return m_state; }
  ServiceStrainState &mutableState() { return m_state; }

private:
  ServiceStrainModel() = default;

  static float clamp100(float value);
  static std::string normalize(std::string value);
  static bool categoryContains(const TileData *tile, const char *needle);

  ServiceStrainState m_state;
};

#endif // SERVICE_STRAIN_MODEL_HXX_
