#include <catch.hpp>

#include "../../src/simulation/BudgetSystem.hxx"
#include "../../src/simulation/GovernanceSystem.hxx"
#include "../../src/simulation/PolicyEngine.hxx"
#include "../../src/simulation/ScenarioCatalog.hxx"

TEST_CASE("ScenarioCatalog loads bundled scenario definitions", "[simulation][scenario]")
{
  ScenarioCatalog &catalog = ScenarioCatalog::instance();
  catalog.loadScenarios();

  const auto &defs = catalog.definitions();
  CHECK(defs.size() >= 5);
  CHECK(catalog.find("balanced_growth") != nullptr);
  CHECK(catalog.find("fiscal_emergency") != nullptr);
}

TEST_CASE("ScenarioCatalog applyScenarioById sets baseline governance, budget, and policies", "[simulation][scenario]")
{
  ScenarioCatalog &catalog = ScenarioCatalog::instance();
  catalog.clear();

  ScenarioDefinition scenario;
  scenario.id = "test_scenario";
  scenario.label = "Test Scenario";
  scenario.startingApproval = 64.f;
  scenario.startingBalance = 1750.f;
  scenario.recommendedPolicies = {"scenario_policy_a"};
  catalog.addDefinition(scenario);

  PolicyEngine &policyEngine = PolicyEngine::instance();
  policyEngine.clearPolicies();

  PolicyDefinition a;
  a.id = "scenario_policy_a";
  a.label = "Scenario Policy A";
  a.levels = {PolicyLevelDefinition{1, 100, 0.f, {}}};
  policyEngine.addDefinition(a);

  PolicyDefinition b;
  b.id = "scenario_policy_b";
  b.label = "Scenario Policy B";
  b.levels = {PolicyLevelDefinition{1, 100, 0.f, {}}};
  policyEngine.addDefinition(b);

  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.clearEvents();
  governance.configure(6, 40.f, 30.f, 15.f, 3, false, false);
  governance.reset();

  BudgetSystem &budget = BudgetSystem::instance();
  budget.reset();

  REQUIRE(catalog.applyScenarioById("test_scenario"));
  CHECK(governance.approval() == Approx(64.f));
  CHECK(budget.currentBalance() == Approx(1750.f));
  CHECK(policyEngine.policyLevel("scenario_policy_a") == 1);
  CHECK(policyEngine.policyLevel("scenario_policy_b") == 0);
}
