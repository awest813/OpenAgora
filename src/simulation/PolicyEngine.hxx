#ifndef POLICY_ENGINE_HXX_
#define POLICY_ENGINE_HXX_

#include "Singleton.hxx"
#include "AffordabilityModel.hxx"
#include "CityIndices.hxx"

#include <string>
#include <vector>

// ── Data structures ──────────────────────────────────────────────────────────

struct PolicyEffect
{
  std::string target; ///< Field name (see DESIGN.md §4.3 for valid targets)
  std::string op;     ///< "add" | "multiply" | "set"
  float value = 0.f;
};

struct PolicyDefinition
{
  std::string id;
  std::string label;
  std::string description;
  std::string type; ///< "budget_sink" | "zone_modifier" | "service_boost" | "tax_modifier"
  int costPerMonth  = 0;
  std::vector<PolicyEffect> effects;

  // UI slider metadata (loaded but interpreted by the UI layer)
  float sliderMin  = 0.f;
  float sliderMax  = 1.f;
  float sliderStep = 1.f;
};

// ── Engine ───────────────────────────────────────────────────────────────────

/**
 * @class PolicyEngine
 * @brief Loads data-driven policy definitions from JSON and applies their
 *        effects each game-month tick to AffordabilityState and CityIndices.
 *
 * Pure C++17 – no SDL headers.
 *
 * Usage:
 * @code
 *   PolicyEngine::instance().loadPolicies();
 *   PolicyEngine::instance().setActive("affordable_housing_fund", true);
 *   // each month:
 *   PolicyEngine::instance().tick(affordabilityState, cityIndicesData);
 * @endcode
 */
class PolicyEngine : public Singleton<PolicyEngine>
{
public:
  friend Singleton<PolicyEngine>;

  /// Load all *.json files from data/resources/data/policies/.
  void loadPolicies();

  /// Enable or disable a policy by ID. Returns false if ID is unknown.
  bool setActive(const std::string &policyId, bool active);

  bool isActive(const std::string &policyId) const;

  /**
   * @brief Apply all active policy effects for one game-month.
   * @param affordabilityState Mutable affordability state to modify.
   * @param cityIndices        Mutable city indices to modify.
   * @return Total budget cost this month (sum of cost_per_month for active policies).
   */
  int tick(AffordabilityState &affordabilityState, CityIndicesData &cityIndices);

  const std::vector<PolicyDefinition> &definitions() const { return m_definitions; }

  void clearPolicies();

  // Test/content helper
  void addDefinition(const PolicyDefinition &definition);

private:
  PolicyEngine() = default;

  static bool applyNumericOp(float &target, const std::string &op, float value);
  static std::string normalizedKey(const std::string &value);

  bool applyEffectToAffordability(const PolicyEffect &effect, AffordabilityState &state);
  bool applyEffectToCityIndices(const PolicyEffect &effect, CityIndicesData &indices);

  std::vector<PolicyDefinition> m_definitions;
  std::vector<bool>             m_active;
};

#endif // POLICY_ENGINE_HXX_
