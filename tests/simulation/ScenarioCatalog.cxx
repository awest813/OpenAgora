#include <catch.hpp>

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
