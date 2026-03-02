#ifndef AFFORDABILITY_MODEL_HXX_
#define AFFORDABILITY_MODEL_HXX_

#include "Singleton.hxx"
#include "tileData.hxx"

#include <string>
#include <vector>

/**
 * @brief Persistent state for the affordability simulation, suitable for save-file serialisation.
 */
struct AffordabilityState
{
  float medianRent            = 0.f; ///< Derived: residential zone density × upkeep multiplier
  float medianIncome          = 50.f; ///< Derived: inhabitant density tiers (LOW/MEDIUM/HIGH)
  float affordabilityIndex    = 50.f; ///< [0, 100] – primary index fed into CityIndices
  float displacementPressure  = 0.f;  ///< [0, 100] – accumulates when affordability < threshold
  float landValueProxy        = 50.f; ///< [0, 100] – rises with density and commercial adjacency
};

/**
 * @class AffordabilityModel
 * @brief Rent / income model with displacement pressure and population churn.
 *
 * Pure C++17 – no SDL headers. Called monthly via GameClock.
 *
 * Tick logic (per §4.2 of DESIGN.md):
 *  1. Compute medianRent from residential zone upkeep density
 *  2. Compute medianIncome from inhabitant density tiers
 *  3. affordabilityIndex = clamp(100 - (rent/income)*50 - displacement*0.3 + policyEffects, 0, 100)
 *  4. If affordabilityIndex < AFFORDABILITY_THRESHOLD: displacementPressure += DISPLACEMENT_RATE
 *     Else: displacementPressure = max(0, displacementPressure - RECOVERY_RATE)
 *  5. If displacementPressure > CHURN_THRESHOLD: population churn flag is set
 */
class AffordabilityModel : public Singleton<AffordabilityModel>
{
public:
  friend Singleton<AffordabilityModel>;

  // ── Configuration ─────────────────────────────────────────────────────────

  struct Config
  {
    float affordabilityThreshold = 40.f;  ///< Index below which displacement accelerates
    float displacementRate       = 3.f;   ///< Pressure increase per month below threshold
    float recoveryRate           = 1.5f;  ///< Pressure decrease per month above threshold
    float churnThreshold         = 60.f;  ///< Pressure level at which population loss begins
    float maxChurnPerTick        = 0.05f; ///< Max fraction of inhabitants lost per month at max pressure
  };

  void configure(const Config &config);
  void reset();

  /**
   * @brief Recompute affordability state from the supplied tile snapshots.
   * @param buildingTiles Non-null TileData* from each occupied BUILDINGS layer node.
   * @param policyAffordabilityBonus Additive bonus from active policies (e.g. Housing Fund).
   */
  void tick(const std::vector<const TileData *> &buildingTiles, float policyAffordabilityBonus = 0.f);

  const AffordabilityState &state() const { return m_state; }
  AffordabilityState &mutableState() { return m_state; }

  /**
   * @brief True if population churn is currently active (displacementPressure > churnThreshold).
   * GamePlay can read this to reduce inhabitant counts on residential nodes.
   */
  bool populationChurnActive() const { return m_churnActive; }

  /**
   * @brief Fraction of inhabitants that should be lost per churn tick (0..maxChurnPerTick).
   */
  float churnRate() const { return m_churnRate; }

private:
  AffordabilityModel() = default;

  static float clamp100(float x) { return x < 0.f ? 0.f : (x > 100.f ? 100.f : x); }

  static float computeMedianRent(const std::vector<const TileData *> &tiles);
  static float computeMedianIncome(const std::vector<const TileData *> &tiles);
  static float computeLandValueProxy(const std::vector<const TileData *> &tiles);

  Config m_config;
  AffordabilityState m_state;
  bool m_churnActive = false;
  float m_churnRate  = 0.f;
};

#endif // AFFORDABILITY_MODEL_HXX_
