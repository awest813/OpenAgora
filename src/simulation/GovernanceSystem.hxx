#ifndef GOVERNANCE_SYSTEM_HXX_
#define GOVERNANCE_SYSTEM_HXX_

#include "Singleton.hxx"
#include "CityIndices.hxx"

#include <string>
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
  GovernanceTrigger trigger;
  int cooldownMonths = 0;
  std::string notification;
  std::vector<GovernanceEffect> effects;
  int monthsUntilReady = 0;
};

struct GovernanceNotification
{
  int month = 0;
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
  bool checkpointPending() const { return m_checkpointPending; }
  int monthsUntilCheckpoint() const;
  int monthCounter() const { return m_totalMonthsElapsed; }
  int maxSelectablePolicies() const { return m_policyConstrained ? 1 : 2; }

  const CityIndicesData &lastIndices() const { return m_lastIndices; }
  const std::vector<GovernancePolicyOption> &policyOptions() const { return m_policyOptions; }
  const std::vector<GovernanceNotification> &recentNotifications() const { return m_notifications; }

  bool setPolicySelection(const std::string &policyId, bool selected);
  void acknowledgeCheckpoint();

  // Test/content helpers
  void clearEvents();
  void addEventDefinition(const GovernanceEventDefinition &definition);

private:
  GovernanceSystem() = default;

  static float clamp100(float value);
  static std::string normalizedKey(const std::string &value);
  static bool applyNumericOp(float &target, const std::string &op, float value);

  float computeApproval(const CityIndicesData &indices) const;
  float valueForTrigger(const std::string &indexKey, const CityIndicesData &indices, float currentApproval) const;
  bool evaluateTrigger(const GovernanceTrigger &trigger, const CityIndicesData &indices, float currentApproval) const;

  bool applyEffectToIndices(const GovernanceEffect &effect, CityIndicesData &indices);
  bool applyEffectToApproval(const GovernanceEffect &effect, float &approvalValue);
  bool applyEffectToGovernanceState(const GovernanceEffect &effect);

  void updatePolicyAvailability();
  int selectedPolicyCount() const;
  void pushNotification(const std::string &message);

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
  bool m_checkpointPending = false;

  std::vector<GovernanceEventDefinition> m_events;
  std::vector<GovernancePolicyOption> m_policyOptions;
  std::vector<GovernanceNotification> m_notifications;
};

#endif // GOVERNANCE_SYSTEM_HXX_
