#include <catch.hpp>

#include "../../src/simulation/GovernanceSystem.hxx"
#include "../../src/simulation/PolicyEngine.hxx"
#include "../../src/simulation/ScenarioCatalog.hxx"

TEST_CASE("Policy content pack loads with leveled definitions", "[simulation][content]")
{
  PolicyEngine &engine = PolicyEngine::instance();
  engine.loadPolicies();

  const auto &defs = engine.definitions();
  REQUIRE(defs.size() >= 20);

  for (const auto &def : defs)
  {
    CHECK_FALSE(def.id.empty());
    CHECK_FALSE(def.label.empty());
    CHECK(!def.levels.empty());
  }
}

TEST_CASE("Governance events load with effects or choices", "[simulation][content]")
{
  GovernanceSystem &governance = GovernanceSystem::instance();
  governance.loadEventDefinitions();
  governance.reset();

  const auto &notesBefore = governance.recentNotifications();
  CHECK(notesBefore.empty());

  // Smoke-check that at least one monthly tick runs with loaded content.
  CityIndicesData indices;
  indices.affordability = 20.f;
  indices.safety = 20.f;
  indices.jobs = 20.f;
  indices.commute = 20.f;
  indices.pollution = 20.f;
  governance.tickMonth(indices);
  SUCCEED();
}

TEST_CASE("Scenario catalog loads curated presets", "[simulation][content]")
{
  ScenarioCatalog &catalog = ScenarioCatalog::instance();
  catalog.loadScenarios();
  REQUIRE(catalog.definitions().size() >= 5);
}
