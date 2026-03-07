#include "GovernanceSystem.hxx"

#include "Constants.hxx"
#include "Filesystem.hxx"
#include "LOG.hxx"
#include "json.hxx"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <filesystem>
#include <sstream>

using json = nlohmann::json;

namespace
{
constexpr int MAX_NOTIFICATIONS = 20;
constexpr const char EVENTS_DIRECTORY[] = "resources/data/events";
} // namespace

float GovernanceSystem::clamp100(float value)
{
  return value < 0.f ? 0.f : (value > 100.f ? 100.f : value);
}

std::string GovernanceSystem::normalizedKey(const std::string &value)
{
  std::string out = value;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool GovernanceSystem::applyNumericOp(float &target, const std::string &op, float value)
{
  const std::string normalizedOp = normalizedKey(op);
  if (normalizedOp == "add")
  {
    target += value;
    return true;
  }

  if (normalizedOp == "multiply")
  {
    target *= value;
    return true;
  }

  if (normalizedOp == "set")
  {
    target = value;
    return true;
  }

  return false;
}

void GovernanceSystem::configure(int checkpointIntervalMonths, float constraintThreshold, float eventThreshold,
                                 float softFailThreshold, int policyLockMonths, bool eventSystemEnabled,
                                 bool councilCheckpointEnabled)
{
  m_checkpointIntervalMonths = std::max(1, checkpointIntervalMonths);
  m_constraintThreshold = clamp100(constraintThreshold);
  m_eventThreshold = clamp100(eventThreshold);
  m_softFailThreshold = clamp100(softFailThreshold);
  m_policyLockMonths = std::max(0, policyLockMonths);
  m_eventSystemEnabled = eventSystemEnabled;
  m_councilCheckpointEnabled = councilCheckpointEnabled;
}

void GovernanceSystem::loadEventDefinitions()
{
  clearEvents();

  const std::filesystem::path eventsDir = std::filesystem::path(fs::getBasePath()) / EVENTS_DIRECTORY;
  if (!std::filesystem::exists(eventsDir) || !std::filesystem::is_directory(eventsDir))
  {
    LOG(LOG_WARNING) << "Governance events directory not found: " << eventsDir.string();
    return;
  }

  std::vector<std::filesystem::path> eventFiles;
  for (const auto &entry : std::filesystem::directory_iterator(eventsDir))
  {
    if (!entry.is_regular_file() || entry.path().extension() != ".json")
      continue;

    eventFiles.emplace_back(entry.path());
  }
  std::sort(eventFiles.begin(), eventFiles.end());

  for (const auto &filePath : eventFiles)
  {
    const std::string raw = fs::readFileAsString(filePath.string());
    const json parsed = json::parse(raw, nullptr, false);

    if (parsed.is_discarded())
    {
      LOG(LOG_WARNING) << "Skipping malformed governance event file: " << filePath.string();
      continue;
    }

    GovernanceEventDefinition eventDef;
    eventDef.id = parsed.value("id", filePath.stem().string());
    eventDef.cooldownMonths = std::max(0, parsed.value("cooldown_months", 0));
    eventDef.notification = parsed.value("notification", std::string{});

    if (parsed.contains("trigger") && parsed["trigger"].is_object())
    {
      eventDef.trigger.index = parsed["trigger"].value("index", std::string{});
      eventDef.trigger.op = parsed["trigger"].value("op", "lt");
      eventDef.trigger.value = parsed["trigger"].value("value", 0.f);
    }

    if (parsed.contains("effects") && parsed["effects"].is_array())
    {
      for (const auto &effectJson : parsed["effects"])
      {
        GovernanceEffect effect;
        effect.target = effectJson.value("target", std::string{});
        effect.op = effectJson.value("op", "add");
        effect.value = effectJson.value("value", 0.f);
        eventDef.effects.emplace_back(effect);
      }
    }

    m_events.emplace_back(eventDef);
  }

  LOG(LOG_INFO) << "GovernanceSystem loaded " << m_events.size() << " event definition(s)";
}

void GovernanceSystem::reset()
{
  m_approval = 50.f;
  m_lastIndices = CityIndicesData{};
  m_totalMonthsElapsed = 0;
  m_monthsSinceCheckpoint = 0;
  m_policyLockMonthsRemaining = 0;
  m_policyConstrained = false;
  m_lostElection = false;
  m_checkpointPending = false;
  m_taxEfficiencyMultiplier = 1.f;
  m_incomeModifier = 1.f;
  m_notifications.clear();

  for (auto &eventDef : m_events)
  {
    eventDef.monthsUntilReady = 0;
  }

  m_policyOptions = {
      {"housing_relief", "Housing Relief", "Subsidise rents and stabilise vulnerable neighbourhoods."},
      {"safety_campaign", "Safety Campaign", "Increase patrol and fire-prevention outreach."},
      {"job_stimulus", "Job Stimulus", "Offer targeted incentives for employers and small business."},
      {"transit_priority", "Transit Priority", "Fund bus lanes and maintenance to ease commutes."},
      {"clean_air_push", "Clean Air Push", "Tighten emissions controls and expand green buffers."},
  };

  updatePolicyAvailability();
}

float GovernanceSystem::computeApproval(const CityIndicesData &indices) const
{
  // Weighted public-trust formula (DESIGN.md §4.5):
  //   publicTrust = 0.30 × affordability
  //               + 0.25 × safety
  //               + 0.20 × jobs
  //               + 0.15 × commute
  //               + 0.10 × (100 - pollution)
  // Pollution is inverted: a high pollution score (bad for citizens) should
  // reduce trust, so we treat "clean air" as the positive contribution.
  return clamp100(0.30f * indices.affordability
                + 0.25f * indices.safety
                + 0.20f * indices.jobs
                + 0.15f * indices.commute
                + 0.10f * (100.f - indices.pollution));
}

float GovernanceSystem::valueForTrigger(const std::string &indexKey, const CityIndicesData &indices, float currentApproval) const
{
  const std::string key = normalizedKey(indexKey);

  if (key == "affordabilityindex" || key == "affordability")
    return indices.affordability;
  if (key == "safetyindex" || key == "safety")
    return indices.safety;
  if (key == "jobsindex" || key == "jobs" || key == "economyindex" || key == "economy")
    return indices.jobs;
  if (key == "commuteindex" || key == "commute")
    return indices.commute;
  if (key == "pollutionindex" || key == "pollution")
    return indices.pollution;
  if (key == "publictrust" || key == "approval")
    return currentApproval;

  return 0.f;
}

bool GovernanceSystem::evaluateTrigger(const GovernanceTrigger &trigger, const CityIndicesData &indices, float currentApproval) const
{
  const float lhs = valueForTrigger(trigger.index, indices, currentApproval);
  const std::string op = normalizedKey(trigger.op);

  if (op == "lt")
    return lhs < trigger.value;
  if (op == "gt")
    return lhs > trigger.value;
  if (op == "eq")
    return std::fabs(lhs - trigger.value) < 0.0001f;

  return false;
}

bool GovernanceSystem::applyEffectToIndices(const GovernanceEffect &effect, CityIndicesData &indices)
{
  const std::string key = normalizedKey(effect.target);
  float *target = nullptr;

  if (key == "affordabilityindex" || key == "affordability")
    target = &indices.affordability;
  else if (key == "safetyindex" || key == "safety")
    target = &indices.safety;
  else if (key == "jobsindex" || key == "jobs" || key == "economyindex" || key == "economy")
    target = &indices.jobs;
  else if (key == "commuteindex" || key == "commute")
    target = &indices.commute;
  else if (key == "pollutionindex" || key == "pollution")
    target = &indices.pollution;

  if (!target)
    return false;

  if (!applyNumericOp(*target, effect.op, effect.value))
    return false;

  *target = clamp100(*target);
  return true;
}

bool GovernanceSystem::applyEffectToApproval(const GovernanceEffect &effect, float &approvalValue)
{
  const std::string key = normalizedKey(effect.target);
  if (key != "publictrust" && key != "approval")
    return false;

  if (!applyNumericOp(approvalValue, effect.op, effect.value))
    return false;

  approvalValue = clamp100(approvalValue);
  return true;
}

bool GovernanceSystem::applyEffectToGovernanceState(const GovernanceEffect &effect)
{
  const std::string key = normalizedKey(effect.target);
  if (key == "policylockmonths" || key == "policylock")
  {
    float lockMonths = static_cast<float>(m_policyLockMonthsRemaining);
    if (!applyNumericOp(lockMonths, effect.op, effect.value))
      return false;

    m_policyLockMonthsRemaining = std::max(0, static_cast<int>(std::round(lockMonths)));
    return true;
  }

  if (key == "taxefficiency")
  {
    if (!applyNumericOp(m_taxEfficiencyMultiplier, effect.op, effect.value))
      return false;
    m_taxEfficiencyMultiplier = std::max(0.1f, std::min(2.f, m_taxEfficiencyMultiplier));
    return true;
  }

  if (key == "medianincome" || key == "income")
  {
    if (!applyNumericOp(m_incomeModifier, effect.op, effect.value))
      return false;
    m_incomeModifier = std::max(0.1f, std::min(2.f, m_incomeModifier));
    return true;
  }

  return false;
}

void GovernanceSystem::tickMonth(const CityIndicesData &indices)
{
  m_totalMonthsElapsed++;
  m_lastIndices = indices;

  if (m_policyLockMonthsRemaining > 0)
  {
    --m_policyLockMonthsRemaining;
  }

  for (auto &eventDef : m_events)
  {
    if (eventDef.monthsUntilReady > 0)
    {
      --eventDef.monthsUntilReady;
    }
  }

  CityIndicesData adjustedIndices = indices;
  float approvalValue = computeApproval(adjustedIndices);

  // Events are evaluated whenever the event system is enabled. Each event
  // defines its own trigger condition (index + threshold). The overall
  // m_eventThreshold acts as a "city distress" guard: events only fire when
  // any individual index OR approval has already fallen to a concerning level.
  // For flexibility, we gate on the *minimum* of approval and any index value
  // rather than approval alone, but still honour the per-event trigger check.
  if (m_eventSystemEnabled)
  {
    const float distressMin = std::min({adjustedIndices.affordability,
                                        adjustedIndices.safety,
                                        adjustedIndices.jobs,
                                        adjustedIndices.commute,
                                        adjustedIndices.pollution,
                                        approvalValue});
    const bool allowEventEvaluation = distressMin <= m_eventThreshold;

    for (auto &eventDef : m_events)
    {
      if (!allowEventEvaluation)
        continue;

      if (eventDef.monthsUntilReady > 0)
        continue;

      if (!evaluateTrigger(eventDef.trigger, adjustedIndices, approvalValue))
        continue;

      eventDef.monthsUntilReady = eventDef.cooldownMonths;
      if (!eventDef.notification.empty())
      {
        pushNotification(eventDef.notification);
      }

      for (const auto &effect : eventDef.effects)
      {
        if (applyEffectToIndices(effect, adjustedIndices))
        {
          approvalValue = computeApproval(adjustedIndices);
          continue;
        }

        if (applyEffectToApproval(effect, approvalValue))
        {
          continue;
        }

        if (applyEffectToGovernanceState(effect))
        {
          continue;
        }

        LOG(LOG_DEBUG) << "Ignoring unsupported governance effect target '" << effect.target << "' in event '" << eventDef.id
                       << "'";
      }
    }
  }

  m_approval = clamp100(approvalValue);
  m_policyConstrained = (m_approval < m_constraintThreshold) || (m_policyLockMonthsRemaining > 0);
  updatePolicyAvailability();

  if (m_councilCheckpointEnabled)
  {
    ++m_monthsSinceCheckpoint;
    if (m_monthsSinceCheckpoint >= m_checkpointIntervalMonths)
    {
      m_monthsSinceCheckpoint = 0;
      m_checkpointPending = true;

      for (auto &policy : m_policyOptions)
      {
        policy.selected = false;
      }

      if (m_approval < m_constraintThreshold)
      {
        m_policyLockMonthsRemaining = std::max(m_policyLockMonthsRemaining, m_policyLockMonths);
        m_policyConstrained = true;
        updatePolicyAvailability();
      }

      if (m_approval < m_softFailThreshold)
      {
        m_lostElection = true;
        pushNotification("Election result: You lost re-election. Sandbox mode continues.");
      }
      else
      {
        m_lostElection = false;
        pushNotification("Election result: Re-elected by city council.");
      }

      std::ostringstream checkpointMsg;
      checkpointMsg << "Council checkpoint: approval " << static_cast<int>(std::round(m_approval)) << "/100.";
      pushNotification(checkpointMsg.str());
    }
  }
}

int GovernanceSystem::monthsUntilCheckpoint() const
{
  if (!m_councilCheckpointEnabled)
    return -1;

  return std::max(0, m_checkpointIntervalMonths - m_monthsSinceCheckpoint);
}

bool GovernanceSystem::setPolicySelection(const std::string &policyId, bool selected)
{
  const auto it = std::find_if(m_policyOptions.begin(), m_policyOptions.end(),
                               [&policyId](const GovernancePolicyOption &option) { return option.id == policyId; });
  if (it == m_policyOptions.end() || !it->enabled)
    return false;

  if (!selected)
  {
    it->selected = false;
    return true;
  }

  if (it->selected)
    return true;

  if (selectedPolicyCount() >= maxSelectablePolicies())
    return false;

  it->selected = true;
  return true;
}

void GovernanceSystem::acknowledgeCheckpoint()
{
  m_checkpointPending = false;
}

void GovernanceSystem::clearEvents()
{
  m_events.clear();
}

void GovernanceSystem::addEventDefinition(const GovernanceEventDefinition &definition)
{
  GovernanceEventDefinition copy = definition;
  copy.monthsUntilReady = 0;
  m_events.push_back(std::move(copy));
}

void GovernanceSystem::updatePolicyAvailability()
{
  for (auto &policy : m_policyOptions)
  {
    policy.enabled = true;
  }

  if (!m_policyConstrained)
    return;

  // Lightweight constraint: only first three policy levers remain available.
  for (size_t i = 0; i < m_policyOptions.size(); ++i)
  {
    if (i < 3)
      continue;

    m_policyOptions[i].enabled = false;
    m_policyOptions[i].selected = false;
  }
}

int GovernanceSystem::selectedPolicyCount() const
{
  return static_cast<int>(std::count_if(m_policyOptions.begin(), m_policyOptions.end(),
                                        [](const GovernancePolicyOption &option) { return option.selected; }));
}

void GovernanceSystem::pushNotification(const std::string &message)
{
  m_notifications.push_back({m_totalMonthsElapsed, message});
  if (m_notifications.size() > MAX_NOTIFICATIONS)
  {
    m_notifications.pop_front();
  }
}
