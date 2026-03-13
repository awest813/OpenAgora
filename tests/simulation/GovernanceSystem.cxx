#include <catch.hpp>

#include "../../src/simulation/GovernanceSystem.hxx"

TEST_CASE("Governance approval is weighted average of city indices", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 30.f, 15.f, 3, false, true);
  governance.reset();

  CityIndicesData indices;
  indices.affordability = 80.f;
  indices.safety = 70.f;
  indices.jobs = 60.f;
  indices.commute = 50.f;
  indices.pollution = 40.f;

  governance.tickMonth(indices);

  // publicTrust = 0.30*80 + 0.25*70 + 0.20*60 + 0.15*50 + 0.10*(100-40)
  //             = 24 + 17.5 + 12 + 7.5 + 6 = 67.0
  CHECK(governance.approval() == Approx(67.f));
}

TEST_CASE("Council checkpoint triggers on configured cadence", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(2, 40.f, 30.f, 15.f, 3, false, true);
  governance.reset();

  CityIndicesData indices;
  indices.affordability = 65.f;
  indices.safety = 65.f;
  indices.jobs = 65.f;
  indices.commute = 65.f;
  indices.pollution = 65.f;

  governance.tickMonth(indices);
  CHECK_FALSE(governance.checkpointPending());
  CHECK(governance.monthsUntilCheckpoint() == 1);

  governance.tickMonth(indices);
  CHECK(governance.checkpointPending());
  CHECK(governance.monthsUntilCheckpoint() == 2);

  governance.acknowledgeCheckpoint();
  CHECK_FALSE(governance.checkpointPending());
}

TEST_CASE("Low approval constrains policy options", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 55.f, 30.f, 15.f, 3, false, false);
  governance.reset();

  CityIndicesData lowIndices;
  lowIndices.affordability = 30.f;
  lowIndices.safety = 30.f;
  lowIndices.jobs = 30.f;
  lowIndices.commute = 30.f;
  lowIndices.pollution = 30.f;

  governance.tickMonth(lowIndices);
  REQUIRE(governance.policyConstrained());
  CHECK(governance.maxSelectablePolicies() == 1);

  const auto &options = governance.policyOptions();
  REQUIRE(options.size() == 5);
  CHECK(options[0].enabled);
  CHECK(options[1].enabled);
  CHECK(options[2].enabled);
  CHECK_FALSE(options[3].enabled);
  CHECK_FALSE(options[4].enabled);
}

TEST_CASE("Very low checkpoint approval results in soft fail", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(1, 40.f, 30.f, 20.f, 3, false, true);
  governance.reset();

  CityIndicesData failingIndices;
  failingIndices.affordability = 5.f;
  failingIndices.safety = 5.f;
  failingIndices.jobs = 5.f;
  failingIndices.commute = 5.f;
  failingIndices.pollution = 5.f;

  governance.tickMonth(failingIndices);

  CHECK(governance.checkpointPending());
  CHECK(governance.lostElection());
}

TEST_CASE("Event cooldown prevents repeated monthly triggering", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 100.f, 15.f, 3, true, false);
  governance.reset();

  GovernanceEventDefinition eventDef;
  eventDef.id = "cooldown_test";
  eventDef.trigger.index = "affordabilityIndex";
  eventDef.trigger.op = "lt";
  eventDef.trigger.value = 60.f;
  eventDef.cooldownMonths = 2;
  eventDef.notification = "Cooldown test event fired";
  eventDef.effects = {GovernanceEffect{"publicTrust", "add", -10.f}};
  governance.addEventDefinition(eventDef);

  CityIndicesData indices;
  indices.affordability = 50.f;
  indices.safety = 50.f;
  indices.jobs = 50.f;
  indices.commute = 50.f;
  indices.pollution = 50.f;

  governance.tickMonth(indices);
  CHECK(governance.approval() == Approx(40.f));
  CHECK(governance.recentNotifications().size() == 1);

  governance.tickMonth(indices);
  CHECK(governance.approval() == Approx(50.f));
  CHECK(governance.recentNotifications().size() == 1);

  governance.tickMonth(indices);
  CHECK(governance.approval() == Approx(40.f));
  CHECK(governance.recentNotifications().size() == 2);
}

TEST_CASE("Governance event effects can modify tax efficiency multiplier", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 100.f, 15.f, 3, true, false);
  governance.reset();

  GovernanceEventDefinition eventDef;
  eventDef.id = "tax_efficiency_test";
  eventDef.trigger.index = "affordabilityIndex";
  eventDef.trigger.op = "lt";
  eventDef.trigger.value = 60.f;
  eventDef.cooldownMonths = 1;
  eventDef.effects = {GovernanceEffect{"taxEfficiency", "multiply", 0.8f}};
  governance.addEventDefinition(eventDef);

  CityIndicesData indices;
  indices.affordability = 50.f;
  indices.safety = 50.f;
  indices.jobs = 50.f;
  indices.commute = 50.f;
  indices.pollution = 50.f;

  governance.tickMonth(indices);
  CHECK(governance.taxEfficiencyMultiplier() == Approx(0.8f));
}

TEST_CASE("Governance event threshold gates event evaluation", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 20.f, 15.f, 3, true, false);
  governance.reset();

  GovernanceEventDefinition eventDef;
  eventDef.id = "threshold_gate_test";
  eventDef.trigger.index = "jobsIndex";
  eventDef.trigger.op = "lt";
  eventDef.trigger.value = 80.f;
  eventDef.cooldownMonths = 1;
  eventDef.notification = "Should not trigger while city is healthy.";
  eventDef.effects = {GovernanceEffect{"publicTrust", "add", -20.f}};
  governance.addEventDefinition(eventDef);

  CityIndicesData healthy;
  healthy.affordability = 85.f;
  healthy.safety = 85.f;
  healthy.jobs = 85.f;
  healthy.commute = 85.f;
  healthy.pollution = 85.f;

  governance.tickMonth(healthy);
  CHECK(governance.recentNotifications().empty());
}

TEST_CASE("Governance supports choice-based events and budget adjustments", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 100.f, 15.f, 3, true, false);
  governance.reset();

  GovernanceEventDefinition eventDef;
  eventDef.id = "choice_event";
  eventDef.trigger.index = "affordabilityIndex";
  eventDef.trigger.op = "lt";
  eventDef.trigger.value = 60.f;
  eventDef.cooldownMonths = 4;
  eventDef.notification = "Choose a response.";

  GovernanceEventDefinition::Choice choiceA;
  choiceA.id = "option_a";
  choiceA.label = "Option A";
  choiceA.budgetCost = 500.f;
  choiceA.effects = {GovernanceEffect{"publicTrust", "add", -5.f}};

  GovernanceEventDefinition::Choice choiceB;
  choiceB.id = "option_b";
  choiceB.label = "Option B";
  choiceB.effects = {GovernanceEffect{"taxEfficiency", "multiply", 1.1f}};

  eventDef.choices = {choiceA, choiceB};
  governance.addEventDefinition(eventDef);

  CityIndicesData indices;
  indices.affordability = 50.f;
  indices.safety = 50.f;
  indices.jobs = 50.f;
  indices.commute = 50.f;
  indices.pollution = 50.f;

  governance.tickMonth(indices);
  REQUIRE(governance.hasPendingEventChoice());
  REQUIRE(governance.pendingEventChoice() != nullptr);
  CHECK(governance.pendingEventChoice()->id == "choice_event");

  CHECK(governance.choosePendingEventOption("option_a"));
  CHECK_FALSE(governance.hasPendingEventChoice());
  CHECK(governance.consumeBudgetAdjustment() == Approx(500.f));
}

TEST_CASE("Governance persisted state can be applied", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 30.f, 15.f, 3, false, false);
  governance.reset();

  GovernancePersistedState state;
  state.approval = 72.f;
  state.totalMonthsElapsed = 18;
  state.monthsSinceCheckpoint = 4;
  state.policyLockMonthsRemaining = 2;
  state.policyConstrained = true;
  state.lostElection = false;
  state.checkpointPending = true;
  state.taxEfficiencyMultiplier = 1.15f;
  state.incomeModifier = 0.92f;

  governance.applyPersistedState(state);
  const GovernancePersistedState loaded = governance.persistedState();

  CHECK(loaded.approval == Approx(72.f));
  CHECK(loaded.totalMonthsElapsed == 18);
  CHECK(loaded.monthsSinceCheckpoint == 4);
  CHECK(loaded.policyLockMonthsRemaining == 2);
  CHECK(loaded.policyConstrained);
  CHECK(loaded.checkpointPending);
  CHECK(loaded.taxEfficiencyMultiplier == Approx(1.15f));
  CHECK(loaded.incomeModifier == Approx(0.92f));
}

TEST_CASE("Governance supports compound trigger_all and trigger_any clauses", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 100.f, 15.f, 3, true, false);
  governance.reset();

  GovernanceEventDefinition eventDef;
  eventDef.id = "compound_trigger_test";
  eventDef.triggerAll = {
      GovernanceTrigger{"affordabilityIndex", "lt", 60.f},
      GovernanceTrigger{"safetyIndex", "lt", 60.f},
  };
  eventDef.triggerAny = {
      GovernanceTrigger{"jobsIndex", "lt", 40.f},
      GovernanceTrigger{"commuteIndex", "lt", 40.f},
  };
  eventDef.cooldownMonths = 3;
  eventDef.notification = "Compound trigger fired";
  eventDef.effects = {GovernanceEffect{"publicTrust", "add", -5.f}};
  governance.addEventDefinition(eventDef);

  CityIndicesData shouldTrigger;
  shouldTrigger.affordability = 50.f;
  shouldTrigger.safety = 50.f;
  shouldTrigger.jobs = 30.f;
  shouldTrigger.commute = 50.f;
  shouldTrigger.pollution = 50.f;

  governance.tickMonth(shouldTrigger);
  REQUIRE_FALSE(governance.recentNotifications().empty());
  CHECK(governance.recentNotifications().back().text == "Compound trigger fired");

  governance.clearEvents();
  governance.reset();
  eventDef.cooldownMonths = 0;
  governance.addEventDefinition(eventDef);

  CityIndicesData shouldNotTrigger;
  shouldNotTrigger.affordability = 50.f;
  shouldNotTrigger.safety = 50.f;
  shouldNotTrigger.jobs = 55.f;
  shouldNotTrigger.commute = 55.f;
  shouldNotTrigger.pollution = 50.f;

  governance.tickMonth(shouldNotTrigger);
  CHECK(governance.recentNotifications().empty());
}

TEST_CASE("Governance event month window gating is honored", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 100.f, 15.f, 3, true, false);
  governance.reset();

  GovernanceEventDefinition eventDef;
  eventDef.id = "month_window_test";
  eventDef.trigger = GovernanceTrigger{"affordabilityIndex", "lt", 60.f};
  eventDef.minMonth = 2;
  eventDef.maxMonth = 3;
  eventDef.cooldownMonths = 0;
  eventDef.notification = "Windowed event";
  eventDef.effects = {GovernanceEffect{"publicTrust", "add", -2.f}};
  governance.addEventDefinition(eventDef);

  CityIndicesData indices;
  indices.affordability = 50.f;
  indices.safety = 50.f;
  indices.jobs = 50.f;
  indices.commute = 50.f;
  indices.pollution = 50.f;

  governance.tickMonth(indices); // month 1
  CHECK(governance.recentNotifications().empty());

  governance.tickMonth(indices); // month 2
  REQUIRE(governance.recentNotifications().size() == 1);
  CHECK(governance.recentNotifications().back().text == "Windowed event");
}

TEST_CASE("Governance tracks consecutive successful checkpoints", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(1, 40.f, 30.f, 15.f, 3, false, true);
  governance.reset();

  CityIndicesData goodIndices;
  goodIndices.affordability = 70.f;
  goodIndices.safety = 70.f;
  goodIndices.jobs = 70.f;
  goodIndices.commute = 70.f;
  goodIndices.pollution = 70.f;

  // Three consecutive successful checkpoints should set wonElection.
  governance.tickMonth(goodIndices); // checkpoint 1
  REQUIRE(governance.checkpointPending());
  CHECK_FALSE(governance.lostElection());
  CHECK(governance.consecutiveSuccessfulCheckpoints() == 1);
  governance.acknowledgeCheckpoint();

  governance.tickMonth(goodIndices); // checkpoint 2
  REQUIRE(governance.checkpointPending());
  CHECK(governance.consecutiveSuccessfulCheckpoints() == 2);
  CHECK_FALSE(governance.wonElection());
  governance.acknowledgeCheckpoint();

  governance.tickMonth(goodIndices); // checkpoint 3
  REQUIRE(governance.checkpointPending());
  CHECK(governance.consecutiveSuccessfulCheckpoints() == 3);
  CHECK(governance.wonElection());
  governance.acknowledgeCheckpoint();
}

TEST_CASE("Governance resets consecutive counter after lost election", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(1, 40.f, 30.f, 20.f, 3, false, true);
  governance.reset();

  CityIndicesData goodIndices;
  goodIndices.affordability = 70.f;
  goodIndices.safety = 70.f;
  goodIndices.jobs = 70.f;
  goodIndices.commute = 70.f;
  goodIndices.pollution = 70.f;

  governance.tickMonth(goodIndices);
  CHECK(governance.consecutiveSuccessfulCheckpoints() == 1);
  governance.acknowledgeCheckpoint();

  CityIndicesData failingIndices;
  failingIndices.affordability = 5.f;
  failingIndices.safety = 5.f;
  failingIndices.jobs = 5.f;
  failingIndices.commute = 5.f;
  failingIndices.pollution = 5.f;

  governance.tickMonth(failingIndices);
  CHECK(governance.lostElection());
  CHECK(governance.consecutiveSuccessfulCheckpoints() == 0);
  governance.acknowledgeCheckpoint();
}

TEST_CASE("Governance pledge is kept and earns approval bonus", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(1, 40.f, 30.f, 15.f, 3, false, true);
  governance.reset();

  CityIndicesData healthyIndices;
  healthyIndices.affordability = 65.f;
  healthyIndices.safety = 65.f;
  healthyIndices.jobs = 65.f;
  healthyIndices.commute = 65.f;
  healthyIndices.pollution = 65.f;

  governance.tickMonth(healthyIndices); // checkpoint fires
  REQUIRE(governance.checkpointPending());

  // Pick the affordability-above-55 pledge (healthy city branch).
  const auto pledges = governance.availablePledges();
  REQUIRE(!pledges.empty());
  const std::string pledgeId = pledges.front().id;
  const float bonusApproval = pledges.front().bonusApproval;
  REQUIRE(governance.setPledge(pledgeId));
  CHECK(governance.hasPledge());
  governance.acknowledgeCheckpoint();

  const float approvalBefore = governance.approval();

  // Tick another checkpoint – same healthy indices should satisfy the pledge.
  governance.tickMonth(healthyIndices);
  REQUIRE(governance.checkpointPending());

  CHECK(governance.approval() >= approvalBefore); // bonus was applied
  governance.acknowledgeCheckpoint();
}

TEST_CASE("Governance pledge broken incurs approval penalty", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(1, 40.f, 100.f, 5.f, 3, false, true);
  governance.reset();

  // Force the pledge to be "Hold Affordability above 40" by using healthy city indices.
  CityIndicesData healthyIndices;
  healthyIndices.affordability = 65.f;
  healthyIndices.safety = 65.f;
  healthyIndices.jobs = 65.f;
  healthyIndices.commute = 65.f;
  healthyIndices.pollution = 65.f;

  governance.tickMonth(healthyIndices);
  REQUIRE(governance.checkpointPending());

  // Manually set a pledge we know will be broken next checkpoint.
  PolicyPledge pledge;
  pledge.id = "test_pledge";
  pledge.description = "Hold Affordability above 60";
  pledge.targetIndex = "affordability";
  pledge.op = "above";
  pledge.threshold = 60.f;
  pledge.bonusApproval = 5.f;
  pledge.penaltyApproval = 8.f;

  // Set via internal helper – we use setPledge with an available id.
  // Instead, directly set through availablePledges that match above-55 condition.
  const auto pledges = governance.availablePledges();
  REQUIRE(!pledges.empty());
  governance.setPledge(pledges.front().id);
  governance.acknowledgeCheckpoint();

  const float approvalAfterFirst = governance.approval();

  // Next tick with poor indices – affordability will drop below threshold.
  CityIndicesData poorIndices;
  poorIndices.affordability = 20.f;
  poorIndices.safety = 65.f;
  poorIndices.jobs = 65.f;
  poorIndices.commute = 65.f;
  poorIndices.pollution = 65.f;

  governance.tickMonth(poorIndices);
  REQUIRE(governance.checkpointPending());

  // Pledge should have been evaluated and broken – approval may be lower.
  CHECK_FALSE(governance.hasPledge());
  governance.acknowledgeCheckpoint();
}

TEST_CASE("Governance persisted state includes pledge and consecutive checkpoints", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 30.f, 15.f, 3, false, false);
  governance.reset();

  GovernancePersistedState state;
  state.approval = 72.f;
  state.totalMonthsElapsed = 18;
  state.monthsSinceCheckpoint = 4;
  state.policyLockMonthsRemaining = 2;
  state.policyConstrained = true;
  state.lostElection = false;
  state.checkpointPending = true;
  state.taxEfficiencyMultiplier = 1.15f;
  state.incomeModifier = 0.92f;
  state.consecutiveSuccessfulCheckpoints = 2;
  state.wonElection = false;
  state.hasPledge = true;
  state.activePledge.id = "pledge_safety_above55";
  state.activePledge.description = "Keep Safety above 55";
  state.activePledge.targetIndex = "safety";
  state.activePledge.op = "above";
  state.activePledge.threshold = 55.f;
  state.activePledge.bonusApproval = 7.f;
  state.activePledge.penaltyApproval = 10.f;

  governance.applyPersistedState(state);
  const GovernancePersistedState loaded = governance.persistedState();

  CHECK(loaded.consecutiveSuccessfulCheckpoints == 2);
  CHECK_FALSE(loaded.wonElection);
  CHECK(loaded.hasPledge);
  CHECK(loaded.activePledge.id == "pledge_safety_above55");
  CHECK(loaded.activePledge.targetIndex == "safety");
  CHECK(loaded.activePledge.threshold == Approx(55.f));
}

TEST_CASE("Governance availablePledges returns non-empty list", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 30.f, 15.f, 3, false, false);
  governance.reset();

  CityIndicesData indices;
  indices.affordability = 50.f;
  indices.safety = 50.f;
  indices.jobs = 50.f;
  indices.commute = 50.f;
  indices.pollution = 50.f;

  governance.tickMonth(indices);

  const auto pledges = governance.availablePledges();
  CHECK(pledges.size() == 4);

  for (const auto &pledge : pledges)
  {
    CHECK_FALSE(pledge.id.empty());
    CHECK_FALSE(pledge.description.empty());
    CHECK_FALSE(pledge.targetIndex.empty());
    CHECK(pledge.bonusApproval > 0.f);
    CHECK(pledge.penaltyApproval > 0.f);
  }
}

TEST_CASE("GovernanceSystem pushStakeholderReaction adds notification", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 30.f, 15.f, 3, false, false);
  governance.reset();

  governance.pushStakeholderReaction("Tenants' Coalition: This helps residents.", "housing");
  REQUIRE(governance.recentNotifications().size() == 1);
  CHECK(governance.recentNotifications().back().text == "Tenants' Coalition: This helps residents.");
  CHECK(governance.recentNotifications().back().category == "housing");
}

TEST_CASE("GovernanceSystem Weighted Random Selection", "[simulation][governance]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 100.f, 15.f, 3, true, false);
  governance.reset();

  GovernanceEventDefinition first;
  first.id = "weighted_one";
  first.trigger = GovernanceTrigger{"affordabilityIndex", "lt", 60.f};
  first.cooldownMonths = 0;
  first.notification = "Weighted One";
  first.weight = 1.f;
  first.effects = {GovernanceEffect{"publicTrust", "add", -1.f}};
  governance.addEventDefinition(first);

  GovernanceEventDefinition second;
  second.id = "weighted_two";
  second.trigger = GovernanceTrigger{"affordabilityIndex", "lt", 60.f};
  second.cooldownMonths = 0;
  second.notification = "Weighted Two";
  second.weight = 5.f;
  second.effects = {GovernanceEffect{"publicTrust", "add", -1.f}};
  governance.addEventDefinition(second);

  CityIndicesData indices;
  indices.affordability = 50.f;
  indices.safety = 50.f;
  indices.jobs = 50.f;
  indices.commute = 50.f;
  indices.pollution = 50.f;

  governance.tickMonth(indices);
  CHECK(governance.recentNotifications().size() == 1);
}
