#ifndef ECONOMY_DEPTH_MODEL_HXX_
#define ECONOMY_DEPTH_MODEL_HXX_

#include "Singleton.hxx"
#include "CityIndices.hxx"
#include "tileData.hxx"

#include <vector>

struct EconomyDepthState
{
  float unemploymentPressure = 50.f; ///< [0..100], higher = worse
  float wagePressure         = 50.f; ///< [0..100], higher = stronger labor pressure
  float businessConfidence   = 50.f; ///< [0..100], higher = stronger investment appetite
  float debtStress           = 0.f;  ///< [0..100], higher = stronger fiscal strain
};

class EconomyDepthModel : public Singleton<EconomyDepthModel>
{
public:
  friend Singleton<EconomyDepthModel>;

  struct Config
  {
    float debtStressScale       = 200.f; ///< balance units per stress point
    float policyCostStressScale = 50.f;  ///< policy expense units per stress point
    float confidenceDebtPenalty = 0.4f;  ///< confidence reduction per debtStress point
  };

  void configure(const Config &config);
  void reset();

  /**
   * @brief Tick deeper economy state once per game-month.
   *
   * @param buildingTiles    Building-layer TileData snapshots.
   * @param indices          City indices snapshot for this month.
   * @param runningBalance   Current city running balance from BudgetSystem.
   * @param policyExpenses   Monthly policy spend.
   * @param approval         Current governance approval [0..100].
   */
  void tick(const std::vector<const TileData *> &buildingTiles, const CityIndicesData &indices, float runningBalance,
            float policyExpenses, float approval);

  const EconomyDepthState &state() const { return m_state; }

private:
  EconomyDepthModel() = default;

  static float clamp100(float value);

  Config m_config;
  EconomyDepthState m_state;
};

#endif // ECONOMY_DEPTH_MODEL_HXX_
