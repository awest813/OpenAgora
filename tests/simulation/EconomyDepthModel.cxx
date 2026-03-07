#include <catch.hpp>

#include "../../src/simulation/EconomyDepthModel.hxx"

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

TEST_CASE("EconomyDepthModel reset returns neutral defaults", "[simulation][economy_depth]")
{
  EconomyDepthModel &model = EconomyDepthModel::instance();
  model.reset();

  const EconomyDepthState &state = model.state();
  CHECK(state.unemploymentPressure == Approx(50.f));
  CHECK(state.wagePressure == Approx(50.f));
  CHECK(state.businessConfidence == Approx(50.f));
  CHECK(state.debtStress == Approx(0.f));
}

TEST_CASE("EconomyDepthModel unemployment improves with more jobs", "[simulation][economy_depth]")
{
  EconomyDepthModel &model = EconomyDepthModel::instance();
  model.reset();

  TileData res = makeTile("Residential", 100);
  TileData com = makeTile("Commercial", 20);
  TileData ind = makeTile("Industrial", 20);

  CityIndicesData indices{};
  indices.jobs = 40.f;
  indices.commute = 50.f;
  indices.safety = 50.f;
  indices.pollution = 50.f;

  model.tick({&res, &com}, indices, 0.f, 0.f, 50.f);
  const float scarceJobsPressure = model.state().unemploymentPressure;

  model.tick({&res, &com, &com, &ind, &ind}, indices, 0.f, 0.f, 50.f);
  const float ampleJobsPressure = model.state().unemploymentPressure;

  CHECK(ampleJobsPressure < scarceJobsPressure);
}

TEST_CASE("EconomyDepthModel debt stress rises with negative balance", "[simulation][economy_depth]")
{
  EconomyDepthModel &model = EconomyDepthModel::instance();
  model.reset();

  TileData res = makeTile("Residential", 50);
  TileData com = makeTile("Commercial", 50);

  CityIndicesData indices{};
  indices.jobs = 60.f;
  indices.commute = 60.f;
  indices.safety = 60.f;
  indices.pollution = 40.f;

  model.tick({&res, &com}, indices, 2000.f, 0.f, 60.f);
  const float positiveBalanceStress = model.state().debtStress;

  model.tick({&res, &com}, indices, -20000.f, 500.f, 60.f);
  const float negativeBalanceStress = model.state().debtStress;

  CHECK(negativeBalanceStress > positiveBalanceStress);
}
