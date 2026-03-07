#include <catch.hpp>

#include "../../src/simulation/ZoneGrowthFormulas.hxx"

// ── Helpers ──────────────────────────────────────────────────────────────────

namespace
{
CityIndicesData makeIndices(float affordability = 50.f, float jobs = 50.f,
                             float safety = 50.f, float commute = 50.f, float pollution = 50.f)
{
  CityIndicesData idx;
  idx.affordability = affordability;
  idx.jobs          = jobs;
  idx.safety        = safety;
  idx.commute       = commute;
  idx.pollution     = pollution;
  return idx;
}

SimulationContextData makeContext(float businessConfidence = 50.f,
                                   float unemploymentPressure = 50.f,
                                   float debtStress = 0.f)
{
  SimulationContextData ctx;
  ctx.businessConfidence   = businessConfidence;
  ctx.unemploymentPressure = unemploymentPressure;
  ctx.debtStress           = debtStress;
  return ctx;
}
} // namespace

// ── Residential multiplier ────────────────────────────────────────────────────

TEST_CASE("ZoneGrowthFormulas: high affordability boosts residential growth", "[simulation][zone_growth]")
{
  const auto highAff = computeZoneGrowthMultipliers(makeIndices(70.f), makeContext(), false);
  const auto lowAff  = computeZoneGrowthMultipliers(makeIndices(25.f), makeContext(), false);
  CHECK(highAff.residential > 1.f);
  CHECK(lowAff.residential < 1.f);
  CHECK(highAff.residential > lowAff.residential);
}

TEST_CASE("ZoneGrowthFormulas: neutral affordability gives residential multiplier 1.0", "[simulation][zone_growth]")
{
  const auto neutral = computeZoneGrowthMultipliers(makeIndices(50.f), makeContext(), false);
  CHECK(neutral.residential == Approx(1.05f));
}

TEST_CASE("ZoneGrowthFormulas: very low affordability applies maximum residential penalty", "[simulation][zone_growth]")
{
  const auto veryLow = computeZoneGrowthMultipliers(makeIndices(20.f), makeContext(), false);
  CHECK(veryLow.residential == Approx(0.80f));
}

// ── Commercial multiplier ─────────────────────────────────────────────────────

TEST_CASE("ZoneGrowthFormulas: high business confidence boosts commercial growth (with economy depth)", "[simulation][zone_growth]")
{
  const auto highConf = computeZoneGrowthMultipliers(makeIndices(), makeContext(70.f, 40.f), true);
  const auto lowConf  = computeZoneGrowthMultipliers(makeIndices(), makeContext(30.f, 40.f), true);
  CHECK(highConf.commercial > 1.f);
  CHECK(lowConf.commercial < 1.f);
  CHECK(highConf.commercial > lowConf.commercial);
}

TEST_CASE("ZoneGrowthFormulas: high unemployment adds penalty on top of confidence penalty", "[simulation][zone_growth]")
{
  const auto lowUnemployment  = computeZoneGrowthMultipliers(makeIndices(), makeContext(30.f, 20.f), true);
  const auto highUnemployment = computeZoneGrowthMultipliers(makeIndices(), makeContext(30.f, 80.f), true);
  // Both have low confidence; high unemployment should compound the penalty
  CHECK(highUnemployment.commercial < lowUnemployment.commercial);
}

TEST_CASE("ZoneGrowthFormulas: commercial fallback uses jobs index when economy depth disabled", "[simulation][zone_growth]")
{
  const auto highJobs = computeZoneGrowthMultipliers(makeIndices(50.f, 70.f), makeContext(), false);
  const auto lowJobs  = computeZoneGrowthMultipliers(makeIndices(50.f, 20.f), makeContext(), false);
  CHECK(highJobs.commercial > 1.f);
  CHECK(lowJobs.commercial < 1.f);
}

// ── Industrial multiplier ─────────────────────────────────────────────────────

TEST_CASE("ZoneGrowthFormulas: high pollution penalises industrial growth", "[simulation][zone_growth]")
{
  const auto highPollution = computeZoneGrowthMultipliers(makeIndices(50.f, 50.f, 50.f, 50.f, 80.f), makeContext(), false);
  const auto lowPollution  = computeZoneGrowthMultipliers(makeIndices(50.f, 50.f, 50.f, 50.f, 20.f), makeContext(), false);
  CHECK(highPollution.industrial < 1.f);
  CHECK(lowPollution.industrial > 1.f);
  CHECK(highPollution.industrial < lowPollution.industrial);
}

TEST_CASE("ZoneGrowthFormulas: high debt stress adds industrial penalty (economy depth)", "[simulation][zone_growth]")
{
  const auto noDebt   = computeZoneGrowthMultipliers(makeIndices(), makeContext(50.f, 50.f, 0.f),  true);
  const auto highDebt = computeZoneGrowthMultipliers(makeIndices(), makeContext(50.f, 50.f, 70.f), true);
  CHECK(highDebt.industrial < noDebt.industrial);
}

TEST_CASE("ZoneGrowthFormulas: debt stress ignored when economy depth disabled", "[simulation][zone_growth]")
{
  const auto noDebt   = computeZoneGrowthMultipliers(makeIndices(), makeContext(50.f, 50.f, 0.f),   false);
  const auto highDebt = computeZoneGrowthMultipliers(makeIndices(), makeContext(50.f, 50.f, 100.f), false);
  CHECK(noDebt.industrial == Approx(highDebt.industrial));
}

// ── Output clamping ───────────────────────────────────────────────────────────

TEST_CASE("ZoneGrowthFormulas: outputs are clamped to [0, 2]", "[simulation][zone_growth]")
{
  // Extreme inputs should never produce out-of-range multipliers
  const auto extremeHigh = computeZoneGrowthMultipliers(
      makeIndices(100.f, 100.f, 100.f, 100.f, 0.f), makeContext(100.f, 0.f, 0.f), true);
  const auto extremeLow = computeZoneGrowthMultipliers(
      makeIndices(0.f, 0.f, 0.f, 0.f, 100.f), makeContext(0.f, 100.f, 100.f), true);

  CHECK(extremeHigh.residential <= 2.f);
  CHECK(extremeHigh.commercial  <= 2.f);
  CHECK(extremeHigh.industrial  <= 2.f);

  CHECK(extremeLow.residential >= 0.f);
  CHECK(extremeLow.commercial  >= 0.f);
  CHECK(extremeLow.industrial  >= 0.f);
}
