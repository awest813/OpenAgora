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
#include <random>
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
    eventDef.category = parsed.value("category", std::string{"general"});
    eventDef.severity = std::max(1, std::min(3, parsed.value("severity", 1)));
    eventDef.cooldownMonths = std::max(0, parsed.value("cooldown_months", 0));
    eventDef.notification = parsed.value("notification", std::string{});

    if (parsed.contains("trigger") && parsed["trigger"].is_object())
    {
      eventDef.trigger.index = parsed["trigger"].value("index", std::string{});
      eventDef.trigger.op = parsed["trigger"].value("op", "lt");
      eventDef.trigger.value = parsed["trigger"].value("value", 0.f);
    }

    if (parsed.contains("trigger_all") && parsed["trigger_all"].is_array())
    {
      for (const auto &triggerJson : parsed["trigger_all"])
      {
        if (!triggerJson.is_object())
          continue;
        GovernanceTrigger trigger;
        trigger.index = triggerJson.value("index", std::string{});
        trigger.op = triggerJson.value("op", "lt");
        trigger.value = triggerJson.value("value", 0.f);
        eventDef.triggerAll.push_back(trigger);
      }
    }

    if (parsed.contains("trigger_any") && parsed["trigger_any"].is_array())
    {
      for (const auto &triggerJson : parsed["trigger_any"])
      {
        if (!triggerJson.is_object())
          continue;
        GovernanceTrigger trigger;
        trigger.index = triggerJson.value("index", std::string{});
        trigger.op = triggerJson.value("op", "lt");
        trigger.value = triggerJson.value("value", 0.f);
        eventDef.triggerAny.push_back(trigger);
      }
    }

    eventDef.minMonth = std::max(0, parsed.value("min_month", 0));
    eventDef.maxMonth = std::max(0, parsed.value("max_month", 0));
    eventDef.weight = std::max(0.f, parsed.value("weight", 1.f));

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

    if (parsed.contains("choices") && parsed["choices"].is_array())
    {
      for (const auto &choiceJson : parsed["choices"])
      {
        if (!choiceJson.is_object())
          continue;

        GovernanceEventDefinition::Choice choice;
        choice.id = choiceJson.value("id", std::string{});
        choice.label = choiceJson.value("label", std::string{"Option"});
        choice.description = choiceJson.value("description", std::string{});
        choice.budgetCost = choiceJson.value("budget_cost", 0.f);

        if (choiceJson.contains("effects") && choiceJson["effects"].is_array())
        {
          for (const auto &choiceEffectJson : choiceJson["effects"])
          {
            GovernanceEffect effect;
            effect.target = choiceEffectJson.value("target", std::string{});
            effect.op = choiceEffectJson.value("op", "add");
            effect.value = choiceEffectJson.value("value", 0.f);
            choice.effects.emplace_back(effect);
          }
        }

        eventDef.choices.emplace_back(choice);
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
  m_wonElection = false;
  m_checkpointPending = false;
  m_taxEfficiencyMultiplier = 1.f;
  m_incomeModifier = 1.f;
  m_hasPendingEventChoice = false;
  m_pendingEventChoice = GovernanceEventDefinition{};
  m_budgetAdjustment = 0.f;
  m_consecutiveSuccessfulCheckpoints = 0;
  m_hasPledge = false;
  m_activePledge = PolicyPledge{};
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
  if (trigger.index.empty())
    return true;

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

bool GovernanceSystem::evaluateEventDefinition(const GovernanceEventDefinition &eventDef, const CityIndicesData &indices,
                                               float currentApproval) const
{
  if (eventDef.minMonth > 0 && m_totalMonthsElapsed < eventDef.minMonth)
    return false;
  if (eventDef.maxMonth > 0 && m_totalMonthsElapsed > eventDef.maxMonth)
    return false;

  if (!eventDef.trigger.index.empty() && !evaluateTrigger(eventDef.trigger, indices, currentApproval))
    return false;

  for (const auto &trigger : eventDef.triggerAll)
  {
    if (!evaluateTrigger(trigger, indices, currentApproval))
      return false;
  }

  if (!eventDef.triggerAny.empty())
  {
    const bool anyMatched = std::any_of(eventDef.triggerAny.begin(), eventDef.triggerAny.end(),
                                        [this, &indices, currentApproval](const GovernanceTrigger &trigger)
                                        { return evaluateTrigger(trigger, indices, currentApproval); });
    if (!anyMatched)
      return false;
  }

  return true;
}

size_t GovernanceSystem::selectWeightedEventIndex(const std::vector<size_t> &eligibleIndices) const
{
  if (eligibleIndices.empty())
    return 0;
  if (eligibleIndices.size() == 1)
    return eligibleIndices.front();

  float totalWeight = 0.f;
  for (const size_t idx : eligibleIndices)
  {
    totalWeight += std::max(0.0001f, m_events[idx].weight);
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> dist(0.f, totalWeight);
  const float pick = dist(rng);

  float cumulative = 0.f;
  for (const size_t idx : eligibleIndices)
  {
    cumulative += std::max(0.0001f, m_events[idx].weight);
    if (pick <= cumulative)
      return idx;
  }

  return eligibleIndices.back();
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
    std::vector<size_t> eligibleEventIndices;
    for (size_t i = 0; i < m_events.size(); ++i)
    {
      auto &eventDef = m_events[i];
      if (!allowEventEvaluation)
        continue;
      if (m_hasPendingEventChoice)
        continue;

      if (eventDef.monthsUntilReady > 0)
        continue;

      if (!evaluateEventDefinition(eventDef, adjustedIndices, approvalValue))
        continue;

      eligibleEventIndices.push_back(i);
    }

    if (!eligibleEventIndices.empty())
    {
      GovernanceEventDefinition &selectedEvent = m_events[selectWeightedEventIndex(eligibleEventIndices)];
      selectedEvent.monthsUntilReady = selectedEvent.cooldownMonths;
      if (!selectedEvent.notification.empty())
      {
        pushNotification(selectedEvent.notification, selectedEvent.category, selectedEvent.severity);
      }

      if (!selectedEvent.choices.empty())
      {
        m_hasPendingEventChoice = true;
        m_pendingEventChoice = selectedEvent;
        pushNotification("Decision required: " + selectedEvent.id, selectedEvent.category, selectedEvent.severity);
      }
      else
      {
        for (const auto &effect : selectedEvent.effects)
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

          LOG(LOG_DEBUG) << "Ignoring unsupported governance effect target '" << effect.target << "' in event '"
                         << selectedEvent.id << "'";
        }
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

      // ── Evaluate pledge from previous term ──────────────────────────────
      if (m_hasPledge)
      {
        const bool kept = evaluatePledge(m_activePledge, adjustedIndices, approvalValue);
        if (kept)
        {
          approvalValue = clamp100(approvalValue + m_activePledge.bonusApproval);
          pushNotification("Pledge kept: " + m_activePledge.description + " (+approval)", "governance", 1);
        }
        else
        {
          approvalValue = clamp100(approvalValue - m_activePledge.penaltyApproval);
          pushNotification("Pledge broken: " + m_activePledge.description + " (-approval)", "governance", 2);
        }
        m_hasPledge = false;
        m_activePledge = PolicyPledge{};

        // Re-sync m_approval so election outcome reflects the pledge modifier.
        m_approval = approvalValue;
        m_policyConstrained = (m_approval < m_constraintThreshold) || (m_policyLockMonthsRemaining > 0);
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
        m_consecutiveSuccessfulCheckpoints = 0;
        clearPledge();
        pushNotification("Election result: You lost re-election. Sandbox mode continues.", "governance", 3);
      }
      else
      {
        m_lostElection = false;
        ++m_consecutiveSuccessfulCheckpoints;

        if (m_consecutiveSuccessfulCheckpoints >= WIN_CONSECUTIVE_CHECKPOINTS && !m_wonElection)
        {
          m_wonElection = true;
          pushNotification("Electoral victory! You have secured three consecutive terms of office.", "governance", 1);
        }

        pushNotification("Election result: Re-elected by city council.", "governance", 1);
      }

      std::ostringstream checkpointMsg;
      checkpointMsg << "Council checkpoint: approval " << static_cast<int>(std::round(m_approval)) << "/100.";
      pushNotification(checkpointMsg.str(), "governance", 2);
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

// ── Pledge API ────────────────────────────────────────────────────────────────

std::vector<PolicyPledge> GovernanceSystem::availablePledges() const
{
  std::vector<PolicyPledge> pledges;

  // Always offer one pledge per index that needs attention.
  if (m_lastIndices.affordability < 60.f)
    pledges.push_back({"pledge_affordability_above40", "Hold Affordability above 40",
                       "affordability", "above", 40.f, 5.f, 8.f});
  else
    pledges.push_back({"pledge_affordability_above55", "Keep Affordability above 55",
                       "affordability", "above", 55.f, 7.f, 10.f});

  if (m_lastIndices.safety < 60.f)
    pledges.push_back({"pledge_safety_above40", "Hold Safety above 40",
                       "safety", "above", 40.f, 5.f, 8.f});
  else
    pledges.push_back({"pledge_safety_above55", "Keep Safety above 55",
                       "safety", "above", 55.f, 7.f, 10.f});

  if (m_lastIndices.jobs < 55.f)
    pledges.push_back({"pledge_jobs_above40", "Grow Jobs above 40",
                       "jobs", "above", 40.f, 5.f, 8.f});
  else
    pledges.push_back({"pledge_jobs_above55", "Sustain Jobs above 55",
                       "jobs", "above", 55.f, 7.f, 10.f});

  if (m_lastIndices.pollution > 55.f)
    pledges.push_back({"pledge_pollution_below65", "Cut Pollution below 65",
                       "pollution", "below", 65.f, 5.f, 8.f});
  else
    pledges.push_back({"pledge_pollution_below45", "Hold Pollution below 45",
                       "pollution", "below", 45.f, 7.f, 10.f});

  return pledges;
}

bool GovernanceSystem::setPledge(const std::string &pledgeId)
{
  const auto pledges = availablePledges();
  const auto it = std::find_if(pledges.begin(), pledges.end(),
                                [&pledgeId](const PolicyPledge &p) { return p.id == pledgeId; });
  if (it == pledges.end())
    return false;

  m_hasPledge = true;
  m_activePledge = *it;
  return true;
}

void GovernanceSystem::clearPledge()
{
  m_hasPledge = false;
  m_activePledge = PolicyPledge{};
}

bool GovernanceSystem::evaluatePledge(const PolicyPledge &pledge, const CityIndicesData &indices,
                                      float currentApproval) const
{
  float value = 0.f;
  const std::string key = normalizedKey(pledge.targetIndex);

  if (key == "affordability")
    value = indices.affordability;
  else if (key == "safety")
    value = indices.safety;
  else if (key == "jobs")
    value = indices.jobs;
  else if (key == "commute")
    value = indices.commute;
  else if (key == "pollution")
    value = indices.pollution;
  else if (key == "approval" || key == "publictrust")
    value = currentApproval;

  const std::string op = normalizedKey(pledge.op);
  if (op == "above")
    return value >= pledge.threshold;
  if (op == "below")
    return value <= pledge.threshold;

  return false;
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

void GovernanceSystem::pushStakeholderReaction(const std::string &reaction, const std::string &category)
{
  if (!reaction.empty())
    pushNotification(reaction, category, 1);
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

void GovernanceSystem::pushNotification(const std::string &message, const std::string &category, int severity)
{
  const int clampedSeverity = std::max(1, std::min(3, severity));
  m_notifications.push_back({m_totalMonthsElapsed, category, clampedSeverity, message});
  if (m_notifications.size() > MAX_NOTIFICATIONS)
  {
    m_notifications.pop_front();
  }
}

const GovernanceEventDefinition *GovernanceSystem::pendingEventChoice() const
{
  return m_hasPendingEventChoice ? &m_pendingEventChoice : nullptr;
}

bool GovernanceSystem::choosePendingEventOption(const std::string &optionId)
{
  if (!m_hasPendingEventChoice)
    return false;

  const auto optionIt = std::find_if(m_pendingEventChoice.choices.begin(), m_pendingEventChoice.choices.end(),
                                     [&optionId](const GovernanceEventDefinition::Choice &option)
                                     { return option.id == optionId; });
  if (optionIt == m_pendingEventChoice.choices.end())
    return false;

  CityIndicesData updatedIndices = m_lastIndices;
  float approvalValue = m_approval;

  for (const auto &effect : optionIt->effects)
  {
    if (applyEffectToIndices(effect, updatedIndices))
    {
      approvalValue = computeApproval(updatedIndices);
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
  }

  m_lastIndices = updatedIndices;
  m_approval = clamp100(approvalValue);
  m_budgetAdjustment += std::max(0.f, optionIt->budgetCost);
  m_hasPendingEventChoice = false;
  pushNotification("Decision enacted: " + optionIt->label, m_pendingEventChoice.category, m_pendingEventChoice.severity);
  m_pendingEventChoice = GovernanceEventDefinition{};
  return true;
}

float GovernanceSystem::consumeBudgetAdjustment()
{
  const float value = m_budgetAdjustment;
  m_budgetAdjustment = 0.f;
  return value;
}

GovernancePersistedState GovernanceSystem::persistedState() const
{
  GovernancePersistedState state;
  state.approval = m_approval;
  state.totalMonthsElapsed = m_totalMonthsElapsed;
  state.monthsSinceCheckpoint = m_monthsSinceCheckpoint;
  state.policyLockMonthsRemaining = m_policyLockMonthsRemaining;
  state.policyConstrained = m_policyConstrained;
  state.lostElection = m_lostElection;
  state.checkpointPending = m_checkpointPending;
  state.taxEfficiencyMultiplier = m_taxEfficiencyMultiplier;
  state.incomeModifier = m_incomeModifier;
  state.consecutiveSuccessfulCheckpoints = m_consecutiveSuccessfulCheckpoints;
  state.wonElection = m_wonElection;
  state.hasPledge = m_hasPledge;
  state.activePledge = m_activePledge;
  return state;
}

void GovernanceSystem::applyPersistedState(const GovernancePersistedState &state)
{
  m_approval = clamp100(state.approval);
  m_totalMonthsElapsed = std::max(0, state.totalMonthsElapsed);
  m_monthsSinceCheckpoint = std::max(0, state.monthsSinceCheckpoint);
  m_policyLockMonthsRemaining = std::max(0, state.policyLockMonthsRemaining);
  m_policyConstrained = state.policyConstrained;
  m_lostElection = state.lostElection;
  m_checkpointPending = state.checkpointPending;
  m_taxEfficiencyMultiplier = std::max(0.1f, std::min(2.f, state.taxEfficiencyMultiplier));
  m_incomeModifier = std::max(0.1f, std::min(2.f, state.incomeModifier));
  m_consecutiveSuccessfulCheckpoints = std::max(0, state.consecutiveSuccessfulCheckpoints);
  m_wonElection = state.wonElection;
  m_hasPledge = state.hasPledge;
  m_activePledge = state.activePledge;
  updatePolicyAvailability();
}
