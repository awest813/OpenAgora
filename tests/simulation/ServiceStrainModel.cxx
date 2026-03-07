#include <catch.hpp>

#include "../../src/simulation/ServiceStrainModel.hxx"

namespace
{
TileData makeTile(const char *category, int inhabitants)
{
  TileData tile;
  tile.category = category;
  tile.inhabitants = inhabitants;
  tile.zoneDensity.push_back(ZoneDensity::MEDIUM);
  return tile;
}
} // namespace

TEST_CASE("ServiceStrainModel reset returns neutral defaults", "[simulation][service_strain]")
{
  ServiceStrainModel &model = ServiceStrainModel::instance();
  model.reset();

  const ServiceStrainState &state = model.state();
  CHECK(state.transitReliability == Approx(50.f));
  CHECK(state.safetyCapacityLoad == Approx(50.f));
  CHECK(state.educationAccessStress == Approx(50.f));
  CHECK(state.healthAccessStress == Approx(50.f));
}

TEST_CASE("ServiceStrainModel transit reliability tracks road coverage", "[simulation][service_strain]")
{
  ServiceStrainModel &model = ServiceStrainModel::instance();
  model.reset();

  CityIndicesData indices{};
  indices.commute = 40.f;
  indices.safety = 50.f;
  indices.jobs = 50.f;
  indices.pollution = 50.f;

  model.tick({}, 2, 200, indices);
  const float lowCoverage = model.state().transitReliability;

  model.tick({}, 20, 200, indices);
  const float highCoverage = model.state().transitReliability;

  CHECK(highCoverage > lowCoverage);
}

TEST_CASE("ServiceStrainModel service load improves with service buildings", "[simulation][service_strain]")
{
  ServiceStrainModel &model = ServiceStrainModel::instance();
  model.reset();

  TileData res = makeTile("Residential", 200);
  TileData emergency = makeTile("Emergency", 120);
  TileData school = makeTile("School", 160);
  TileData health = makeTile("Health", 140);

  CityIndicesData indices{};
  indices.commute = 60.f;
  indices.safety = 30.f;
  indices.jobs = 45.f;
  indices.pollution = 50.f;

  model.tick({&res}, 20, 200, indices);
  const ServiceStrainState sparse = model.state();

  model.tick({&res, &emergency, &school, &health}, 20, 200, indices);
  const ServiceStrainState covered = model.state();

  CHECK(covered.safetyCapacityLoad < sparse.safetyCapacityLoad);
  CHECK(covered.educationAccessStress < sparse.educationAccessStress);
  CHECK(covered.healthAccessStress < sparse.healthAccessStress);
}
