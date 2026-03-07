#ifndef POLICY_ENGINE_HXX_
#define POLICY_ENGINE_HXX_

#include "Singleton.hxx"
#include "AffordabilityModel.hxx"
#include "CityIndices.hxx"
#include "SimulationContext.hxx"

#include <string>
#include <vector>

// ── Data structures ──────────────────────────────────────────────────────────

struct PolicyEffect
{
  std::string target; ///< Field name (see DESIGN.md §4.3 for valid targets)
  std::string op;     ///< "add" | "multiply" | "set"
  float value = 0.f;
};

struct PolicyLevelDefinition
{
  int level = 1;
  int costPerMonth = 0;
  float unlockMinApproval = 0.f;
  std::vector<PolicyEffect> effects;
};

struct PolicyAvailability
{
  bool available = true;
  std::string reason;
};

struct PolicyDefinition
{
  std::string id;
  std::string label;
  std::string description;
  std::string category; ///< "housing", "transit", "safety", "environment", ...
  std::string type; ///< "budget_sink" | "zone_modifier" | "service_boost" | "tax_modifier"
  int costPerMonth  = 0;
  float minApproval = 0.f;
  int durationMonths = 0; ///< Optional temporary policy duration (0 = permanent while active)
  std::vector<std::string> requires;
  std::vector<std::string> exclusiveWith;
  std::vector<PolicyEffect> effects;
  std::vector<PolicyLevelDefinition> levels;

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
 *   PolicyEngine::instance().setPolicyLevel("affordable_housing_fund", 1);
 *   // each month:
 *   PolicyEngine::instance().tick(affordabilityState, cityIndicesData, &context);
 * @endcode
 */
class PolicyEngine : public Singleton<PolicyEngine>
{
public:
  friend Singleton<PolicyEngine>;

  /// Load all *.json files from data/resources/data/policies/.
  void loadPolicies();

  /// Backward-compatible toggle API (maps false->level 0, true->level 1).
  bool setActive(const std::string &policyId, bool active);
  bool isActive(const std::string &policyId) const;

  /// Select a policy level (0 = disabled). Returns false if policy or level is invalid.
  bool setPolicyLevel(const std::string &policyId, int level);
  int policyLevel(const std::string &policyId) const;
  int maxLevel(const std::string &policyId) const;

  /// Evaluate whether a requested level can be enabled under current conditions.
  PolicyAvailability availability(const std::string &policyId, int requestedLevel, float approval) const;

  /**
   * @brief Apply all active policy effects for one game-month.
   * @param affordabilityState Mutable affordability state to modify.
   * @param cityIndices        Mutable city indices to modify.
   * @param simContext         Optional shared simulation context for deep targets.
   * @return Total budget cost this month (sum of cost_per_month for active policies).
   */
  int tick(AffordabilityState &affordabilityState, CityIndicesData &cityIndices,
           SimulationContextData *simContext = nullptr);

  const std::vector<PolicyDefinition> &definitions() const { return m_definitions; }

  void clearPolicies();

  // Test/content helper
  void addDefinition(const PolicyDefinition &definition);

private:
  PolicyEngine() = default;

  static bool applyNumericOp(float &target, const std::string &op, float value);
  static std::string normalizedKey(const std::string &value);
  static bool isSupportedTarget(const std::string &target);

  bool applyEffectToAffordability(const PolicyEffect &effect, AffordabilityState &state);
  bool applyEffectToCityIndices(const PolicyEffect &effect, CityIndicesData &indices);
  bool applyEffectToSimulationContext(const PolicyEffect &effect, SimulationContextData &context);

  const PolicyLevelDefinition *resolveLevelDefinition(const PolicyDefinition &definition, int level) const;
  int findPolicyIndex(const std::string &policyId) const;

  std::vector<PolicyDefinition> m_definitions;
  std::vector<int> m_selectedLevel;
  std::vector<int> m_remainingDurationMonths;
};

#endif // POLICY_ENGINE_HXX_
