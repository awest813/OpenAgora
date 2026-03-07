#ifndef SIMULATION_CONTEXT_HXX_
#define SIMULATION_CONTEXT_HXX_

#include "Singleton.hxx"

/**
 * @brief Cross-system monthly simulation state shared by depth models.
 *
 * This lightweight context carries high-level values produced by multiple
 * systems (governance, economy, services) so UI and other simulation modules
 * can read a coherent "city pulse" snapshot without reaching into each model.
 */
struct SimulationContextData
{
  int month = 0;

  float taxEfficiency       = 1.f;
  float approvalMultiplier  = 1.f;
  float growthRateModifier  = 1.f;

  float unemploymentPressure = 50.f; ///< [0..100], higher = worse
  float wagePressure         = 50.f; ///< [0..100], higher = stronger wage demand
  float businessConfidence   = 50.f; ///< [0..100], higher = stronger investment appetite
  float debtStress           = 0.f;  ///< [0..100], higher = stronger fiscal strain

  float transitReliability    = 50.f; ///< [0..100], higher = more reliable service
  float safetyCapacityLoad    = 50.f; ///< [0..100], higher = more overloaded
  float educationAccessStress = 50.f; ///< [0..100], higher = poorer access
  float healthAccessStress    = 50.f; ///< [0..100], higher = poorer access
};

class SimulationContext : public Singleton<SimulationContext>
{
public:
  friend Singleton<SimulationContext>;

  void reset();
  void advanceMonth();

  const SimulationContextData &data() const { return m_data; }
  SimulationContextData &mutableData() { return m_data; }

private:
  SimulationContext() = default;
  SimulationContextData m_data;
};

#endif // SIMULATION_CONTEXT_HXX_
