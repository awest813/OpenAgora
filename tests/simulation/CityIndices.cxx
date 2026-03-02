#include <catch.hpp>

#include "../../src/simulation/CityIndices.hxx"

// ── Helpers ────────────────────────────────────────────────────────────────

static TileData makeTile(const char *category, int inhabitants, int pollution, int crime, int fire,
                         int upkeep, ZoneDensity density)
{
  TileData td;
  td.category      = category;
  td.inhabitants   = inhabitants;
  td.pollutionLevel = pollution;
  td.crimeLevel    = crime;
  td.fireHazardLevel = fire;
  td.upkeepCost    = upkeep;
  td.zoneDensity.push_back(density);
  return td;
}

// ── Tests ───────────────────────────────────────────────────────────────────

TEST_CASE("CityIndices returns neutral defaults on empty map", "[simulation][city_indices]")
{
  CityIndices &ci = CityIndices::instance();
  ci.tick({}, 0, 100);

  const CityIndicesData &cur = ci.current();
  CHECK(cur.affordability == Approx(50.f));
  CHECK(cur.safety        == Approx(70.f)); // empty city is safe
  CHECK(cur.jobs          == Approx(50.f));
  CHECK(cur.commute       == Approx(0.f));  // no roads
  CHECK(cur.pollution     == Approx(100.f)); // no polluters
}

TEST_CASE("Affordability index degrades with high-density residential", "[simulation][city_indices]")
{
  CityIndices &ci = CityIndices::instance();

  TileData lowDensity  = makeTile("Residential", 10, 0, 0, 0, 5,  ZoneDensity::LOW);
  TileData highDensity = makeTile("Residential", 80, 0, 0, 0, 50, ZoneDensity::HIGH);

  {
    std::vector<const TileData *> allLow = {&lowDensity, &lowDensity, &lowDensity};
    ci.tick(allLow, 10, 100);
    const float lowScore = ci.current().affordability;

    std::vector<const TileData *> allHigh = {&highDensity, &highDensity, &highDensity};
    ci.tick(allHigh, 10, 100);
    const float highScore = ci.current().affordability;

    CHECK(lowScore > highScore);
    CHECK(lowScore == Approx(100.f));
    CHECK(highScore == Approx(0.f));
  }
}

TEST_CASE("Safety index is lower with high crime and fire hazard", "[simulation][city_indices]")
{
  CityIndices &ci = CityIndices::instance();

  TileData safeTile = makeTile("Residential", 10, 0, 0, 0, 5, ZoneDensity::LOW);
  TileData crimeTile = makeTile("Residential", 10, 0, 40, 20, 5, ZoneDensity::LOW);

  {
    std::vector<const TileData *> safeCity = {&safeTile, &safeTile};
    ci.tick(safeCity, 5, 50);
    const float safeScore = ci.current().safety;

    std::vector<const TileData *> crimeCity = {&crimeTile, &crimeTile};
    ci.tick(crimeCity, 5, 50);
    const float crimeScore = ci.current().safety;

    CHECK(safeScore > crimeScore);
  }
}

TEST_CASE("Jobs index rises when commercial/industrial tiles exceed residential", "[simulation][city_indices]")
{
  CityIndices &ci = CityIndices::instance();

  TileData res  = makeTile("Residential", 20, 0, 0, 0, 10, ZoneDensity::LOW);
  TileData com  = makeTile("Commercial",   5, 0, 0, 0, 30, ZoneDensity::MEDIUM);

  // Residential-heavy city: 3 res tiles, 1 commercial → low employment ratio
  std::vector<const TileData *> resHeavy = {&res, &res, &res, &com};
  ci.tick(resHeavy, 5, 50);
  const float jobsScarce = ci.current().jobs;

  // Employment-heavy: 1 res, 4 commercial → high ratio
  std::vector<const TileData *> jobRich = {&res, &com, &com, &com, &com};
  ci.tick(jobRich, 5, 50);
  const float jobsAbundant = ci.current().jobs;

  CHECK(jobsAbundant > jobsScarce);
}

TEST_CASE("Commute index scales with road coverage", "[simulation][city_indices]")
{
  CityIndices &ci = CityIndices::instance();

  ci.tick({}, 0, 200);
  const float noRoads = ci.current().commute;

  ci.tick({}, 10, 200);   // 5% coverage → score 100
  const float someRoads = ci.current().commute;

  ci.tick({}, 20, 200);   // 10% → capped at 100
  const float moreRoads = ci.current().commute;

  CHECK(someRoads > noRoads);
  CHECK(moreRoads >= someRoads);
  CHECK(moreRoads == Approx(100.f));
}

TEST_CASE("Pollution index degrades as polluting tiles accumulate", "[simulation][city_indices]")
{
  CityIndices &ci = CityIndices::instance();

  TileData clean  = makeTile("Residential", 10, 0,  0, 0, 5, ZoneDensity::LOW);
  TileData dirty  = makeTile("Industrial",  20, 80, 5, 5, 20, ZoneDensity::HIGH);

  std::vector<const TileData *> cleanCity = {&clean, &clean};
  ci.tick(cleanCity, 0, 100);
  const float cleanScore = ci.current().pollution;

  std::vector<const TileData *> dirtyCity = {&dirty, &dirty, &dirty, &dirty, &dirty};
  ci.tick(dirtyCity, 0, 100);
  const float dirtyScore = ci.current().pollution;

  CHECK(cleanScore > dirtyScore);
}

TEST_CASE("Previous snapshot captured on each tick", "[simulation][city_indices]")
{
  CityIndices &ci = CityIndices::instance();

  TileData res = makeTile("Residential", 10, 0, 0, 0, 5, ZoneDensity::LOW);
  std::vector<const TileData *> tiles = {&res};

  ci.tick(tiles, 5, 100);
  const float firstAfford = ci.current().affordability;

  TileData highDensity = makeTile("Residential", 80, 0, 0, 0, 50, ZoneDensity::HIGH);
  std::vector<const TileData *> highTiles = {&highDensity};
  ci.tick(highTiles, 5, 100);

  CHECK(ci.previous().affordability == Approx(firstAfford));
  CHECK(ci.current().affordability  != Approx(firstAfford));
}
