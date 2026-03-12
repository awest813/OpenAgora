#ifndef GOVERNANCE_SYSTEM_HXX_
#define GOVERNANCE_SYSTEM_HXX_

#include "Singleton.hxx"
#include "CityIndices.hxx"

#include <string>
#include <deque>
#include <vector>

struct GovernanceEffect
{
  std::string target;
  std::string op;
  float value = 0.f;
};

struct GovernanceTrigger
{
  std::string index;
  std::string op = "lt";
  float value = 0.f;
};

struct GovernanceEventDefinition
{
  std::string id;
  std::string category = "general";
  int severity = 1; ///< 1=low,2=medium,3=high
  GovernanceTrigger trigger;
  std::vector<GovernanceTrigger> triggerAll;
  std::vector<GovernanceTrigger> triggerAny;
  int minMonth = 0;
  int maxMonth = 0; ///< 0 means no upper bound
  float weight = 1.f;
  int cooldownMonths = 0;
  std::string notification;
  std::vector<GovernanceEffect> effects;
  struct Choice
  {
    std::string id;
    std::string label;
    std::string description;
    float budgetCost = 0.f;
    std::vector<GovernanceEffect> effects;
  };
  std::vector<Choice> choices;
  int monthsUntilReady = 0;
};

struct GovernanceNotification
{
  int month = 0;
  std::string category = "general";
  int severity = 1;
  std::string text;
};

struct GovernancePolicyOption
{
  std::string id;
  std::string label;
  std::string description;
  bool enabled = true;
  bool selected = false;
};

/// A pledge the mayor makes at a council checkpoint.
struct PolicyPledge
{
  std::string id;
  std::string description;   ///< Human-readable promise shown in UI
  std::string targetIndex;   ///< "affordability" | "safety" | "jobs" | "commute" | "pollution"
  std::string op;            ///< "above" | "below"
  float threshold = 0.f;
  float bonusApproval  = 5.f;  ///< Approval awarded when pledge is kept at next checkpoint
  float penaltyApproval = 8.f; ///< Approval lost when pledge is broken at next checkpoint
};

struct GovernancePersistedState
{
  float approval = 50.f;
  int totalMonthsElapsed = 0;
  int monthsSinceCheckpoint = 0;
  int policyLockMonthsRemaining = 0;
  bool policyConstrained = false;
  bool lostElection = false;
  bool checkpointPending = false;
  float taxEfficiencyMultiplier = 1.f;
  float incomeModifier = 1.f;
  int consecutiveSuccessfulCheckpoints = 0;
  bool wonElection = false;
  bool hasPledge = false;
  PolicyPledge activePledge;
};

/**
 * @class GovernanceSystem
 * @brief Lightweight monthly governance loop with elections/events.
 *
 * Pure C++17 simulation module (no SDL dependencies). Consumes CityIndices,
 * computes public approval, runs council checkpoints every X months, constrains
 * policy options under low approval, and triggers cooldown-based civic events.
 */
class GovernanceSystem : public Singleton<GovernanceSystem>
{
public:
  friend Singleton<GovernanceSystem>;

  void configure(int checkpointIntervalMonths, float constraintThreshold, float eventThreshold, float softFailThreshold,
                 int policyLockMonths, bool eventSystemEnabled, bool councilCheckpointEnabled);
  void loadEventDefinitions();
  void reset();

  void tickMonth(const CityIndicesData &indices);

  float approval() const { return m_approval; }
  bool policyConstrained() const { return m_policyConstrained; }
  bool lostElection() const { return m_lostElection; }
  bool wonElection() const { return m_wonElection; }
  bool checkpointPending() const { return m_checkpointPending; }
  float taxEfficiencyMultiplier() const { return m_taxEfficiencyMultiplier; }
  float incomeModifier() const { return m_incomeModifier; }
  int consecutiveSuccessfulCheckpoints() const { return m_consecutiveSuccessfulCheckpoints; }
  bool hasPendingEventChoice() const { return m_hasPendingEventChoice; }
  const GovernanceEventDefinition *pendingEventChoice() const;
  bool choosePendingEventOption(const std::string &optionId);
  float consumeBudgetAdjustment();
  GovernancePersistedState persistedState() const;
  void applyPersistedState(const GovernancePersistedState &state);
  int monthsUntilCheckpoint() const;
  int monthCounter() const { return m_totalMonthsElapsed; }
  int maxSelectablePolicies() const { return m_policyConstrained ? 1 : 2; }

  const CityIndicesData &lastIndices() const { return m_lastIndices; }
  const std::vector<GovernancePolicyOption> &policyOptions() const { return m_policyOptions; }
  const std::deque<GovernanceNotification> &recentNotifications() const { return m_notifications; }

  bool setPolicySelection(const std::string &policyId, bool selected);
  void acknowledgeCheckpoint();

  // ── Pledge API ────────────────────────────────────────────────────────────
  /// Returns up to 4 pledge choices appropriate for the current city state.
  std::vector<PolicyPledge> availablePledges() const;
  /// Commit to a pledge; returns false if the pledge id is not in availablePledges().
  bool setPledge(const std::string &pledgeId);
  /// Clear any active pledge (e.g. when election is lost).
  void clearPledge();
  bool hasPledge() const { return m_hasPledge; }
  const PolicyPledge &activePledge() const { return m_activePledge; }

  // Test/content helpers
  void clearEvents();
  void addEventDefinition(const GovernanceEventDefinition &definition);
  /// Push a stakeholder reaction notification (called by PolicyPanel on policy toggle).
  void pushStakeholderReaction(const std::string &reaction, const std::string &category = "policy");

private:
  GovernanceSystem() = default;

  static float clamp100(float value);
  static std::string normalizedKey(const std::string &value);
  static bool applyNumericOp(float &target, const std::string &op, float value);

  float computeApproval(const CityIndicesData &indices) const;
  float valueForTrigger(const std::string &indexKey, const CityIndicesData &indices, float currentApproval) const;
  bool evaluateTrigger(const GovernanceTrigger &trigger, const CityIndicesData &indices, float currentApproval) const;
  bool evaluateEventDefinition(const GovernanceEventDefinition &eventDef, const CityIndicesData &indices,
                               float currentApproval) const;
  size_t selectWeightedEventIndex(const std::vector<size_t> &eligibleIndices) const;

  bool applyEffectToIndices(const GovernanceEffect &effect, CityIndicesData &indices);
  bool applyEffectToApproval(const GovernanceEffect &effect, float &approvalValue);
  bool applyEffectToGovernanceState(const GovernanceEffect &effect);
  bool evaluatePledge(const PolicyPledge &pledge, const CityIndicesData &indices, float currentApproval) const;

  void updatePolicyAvailability();
  int selectedPolicyCount() const;
  void pushNotification(const std::string &message, const std::string &category = "general", int severity = 1);

  int m_checkpointIntervalMonths = 6;
  float m_constraintThreshold = 40.f;
  float m_eventThreshold = 30.f;
  float m_softFailThreshold = 15.f;
  int m_policyLockMonths = 3;
  bool m_eventSystemEnabled = false;
  bool m_councilCheckpointEnabled = false;

  float m_approval = 50.f;
  CityIndicesData m_lastIndices;

  int m_totalMonthsElapsed = 0;
  int m_monthsSinceCheckpoint = 0;
  int m_policyLockMonthsRemaining = 0;
  bool m_policyConstrained = false;
  bool m_lostElection = false;
  bool m_wonElection = false;
  bool m_checkpointPending = false;
  float m_taxEfficiencyMultiplier = 1.f;
  float m_incomeModifier = 1.f;
  bool m_hasPendingEventChoice = false;
  GovernanceEventDefinition m_pendingEventChoice;
  float m_budgetAdjustment = 0.f;

  int m_consecutiveSuccessfulCheckpoints = 0;
  static constexpr int WIN_CONSECUTIVE_CHECKPOINTS = 3;
  bool m_hasPledge = false;
  PolicyPledge m_activePledge;

  std::vector<GovernanceEventDefinition> m_events;
  std::vector<GovernancePolicyOption> m_policyOptions;
  std::deque<GovernanceNotification> m_notifications;
};

#endif // GOVERNANCE_SYSTEM_HXX_
