#include <catch.hpp>

#include "../../src/simulation/GovernanceSystem.hxx"

TEST_CASE("Governance approval is average of city indices", "[simulation][governance]")
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

  CHECK(governance.approval() == Approx(60.f));
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
