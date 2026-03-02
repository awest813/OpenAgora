#include <catch.hpp>

#include "../../src/simulation/AffordabilityModel.hxx"

// ── Helpers ────────────────────────────────────────────────────────────────

static TileData makeResTile(int inhabitants, int upkeep, ZoneDensity density)
{
  TileData td;
  td.category      = "Residential";
  td.inhabitants   = inhabitants;
  td.upkeepCost    = upkeep;
  td.pollutionLevel = 0;
  td.crimeLevel    = 0;
  td.fireHazardLevel = 0;
  td.zoneDensity.push_back(density);
  return td;
}

static TileData makeComTile(int inhabitants)
{
  TileData td;
  td.category    = "Commercial";
  td.inhabitants = inhabitants;
  td.upkeepCost  = 30;
  td.zoneDensity.push_back(ZoneDensity::MEDIUM);
  return td;
}

// ── Tests ───────────────────────────────────────────────────────────────────

TEST_CASE("AffordabilityModel starts at neutral state after reset", "[simulation][affordability]")
{
  AffordabilityModel &model = AffordabilityModel::instance();
  model.reset();

  const AffordabilityState &s = model.state();
  CHECK(s.affordabilityIndex   == Approx(50.f));
  CHECK(s.displacementPressure == Approx(0.f));
  CHECK(s.medianIncome         == Approx(50.f));
  CHECK(model.populationChurnActive() == false);
  CHECK(model.churnRate() == Approx(0.f));
}

TEST_CASE("High-density residential raises rent, lowers affordability", "[simulation][affordability]")
{
  AffordabilityModel &model = AffordabilityModel::instance();
  model.reset();

  TileData lowDensity  = makeResTile(10,  5, ZoneDensity::LOW);
  TileData highDensity = makeResTile(80, 60, ZoneDensity::HIGH);

  model.tick({&lowDensity, &lowDensity});
  const float affordLow = model.state().affordabilityIndex;

  model.reset();
  model.tick({&highDensity, &highDensity});
  const float affordHigh = model.state().affordabilityIndex;

  CHECK(affordLow > affordHigh);
}

TEST_CASE("Policy bonus increases affordability", "[simulation][affordability]")
{
  AffordabilityModel &model = AffordabilityModel::instance();
  model.reset();

  TileData tile = makeResTile(40, 50, ZoneDensity::MEDIUM);
  std::vector<const TileData *> tiles = {&tile, &tile};

  model.tick(tiles, 0.f);
  const float baseAfford = model.state().affordabilityIndex;

  model.reset();
  model.tick(tiles, 20.f); // +20 policy bonus
  const float boostedAfford = model.state().affordabilityIndex;

  CHECK(boostedAfford > baseAfford);
  CHECK(boostedAfford <= 100.f);
}

TEST_CASE("Displacement pressure accumulates when affordability is low", "[simulation][affordability]")
{
  AffordabilityModel &model = AffordabilityModel::instance();
  AffordabilityModel::Config cfg;
  cfg.affordabilityThreshold = 80.f; // very high threshold so pressure always rises
  cfg.displacementRate       = 5.f;
  cfg.recoveryRate           = 1.f;
  cfg.churnThreshold         = 60.f;
  cfg.maxChurnPerTick        = 0.05f;
  model.configure(cfg);
  model.reset();

  TileData tile = makeResTile(80, 80, ZoneDensity::HIGH);
  std::vector<const TileData *> tiles = {&tile, &tile, &tile};

  model.tick(tiles);
  const float pressure1 = model.state().displacementPressure;
  model.tick(tiles);
  const float pressure2 = model.state().displacementPressure;

  CHECK(pressure2 > pressure1);
}

TEST_CASE("Population churn activates when pressure exceeds churn threshold", "[simulation][affordability]")
{
  AffordabilityModel &model = AffordabilityModel::instance();
  AffordabilityModel::Config cfg;
  cfg.affordabilityThreshold = 99.f; // almost always below threshold
  cfg.displacementRate       = 35.f; // fast pressure rise
  cfg.recoveryRate           = 0.f;
  cfg.churnThreshold         = 60.f;
  cfg.maxChurnPerTick        = 0.05f;
  model.configure(cfg);
  model.reset();

  TileData tile = makeResTile(80, 80, ZoneDensity::HIGH);
  std::vector<const TileData *> tiles = {&tile, &tile};

  // Tick enough times to exceed churn threshold
  for (int i = 0; i < 5; ++i)
    model.tick(tiles);

  CHECK(model.populationChurnActive() == true);
  CHECK(model.churnRate() > 0.f);
  CHECK(model.churnRate() <= 0.05f);
}

TEST_CASE("Displacement pressure recovers when affordability improves", "[simulation][affordability]")
{
  AffordabilityModel &model = AffordabilityModel::instance();
  AffordabilityModel::Config cfg;
  cfg.affordabilityThreshold = 99.f;
  cfg.displacementRate       = 20.f;
  cfg.recoveryRate           = 5.f;
  cfg.churnThreshold         = 60.f;
  cfg.maxChurnPerTick        = 0.05f;
  model.configure(cfg);
  model.reset();

  TileData highTile = makeResTile(80, 80, ZoneDensity::HIGH);
  TileData lowTile  = makeResTile(10,  5, ZoneDensity::LOW);

  // Build up pressure first
  model.tick({&highTile, &highTile});
  model.tick({&highTile, &highTile});
  const float pressureBuilt = model.state().displacementPressure;

  // Now switch to affordable housing + big policy bonus → pressure recovers
  model.tick({&lowTile, &lowTile}, 50.f);
  const float pressureAfterRecovery = model.state().displacementPressure;

  CHECK(pressureAfterRecovery < pressureBuilt);
}

TEST_CASE("Jobs from commercial tiles boost median income", "[simulation][affordability]")
{
  AffordabilityModel &model = AffordabilityModel::instance();
  model.reset();

  TileData res = makeResTile(30, 10, ZoneDensity::LOW);
  TileData com = makeComTile(100);

  model.tick({&res});
  const float incomeNoJobs = model.state().medianIncome;

  model.reset();
  model.tick({&res, &com, &com, &com});
  const float incomeWithJobs = model.state().medianIncome;

  CHECK(incomeWithJobs > incomeNoJobs);
}
