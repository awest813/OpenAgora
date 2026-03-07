#include <catch.hpp>

#include "../../src/simulation/SimulationContext.hxx"

TEST_CASE("SimulationContext reset returns default values", "[simulation][context]")
{
  SimulationContext &ctx = SimulationContext::instance();
  ctx.reset();

  const SimulationContextData &state = ctx.data();
  CHECK(state.month == 0);
  CHECK(state.taxEfficiency == Approx(1.f));
  CHECK(state.approvalMultiplier == Approx(1.f));
  CHECK(state.growthRateModifier == Approx(1.f));
  CHECK(state.businessConfidence == Approx(50.f));
  CHECK(state.debtStress == Approx(0.f));
}

TEST_CASE("SimulationContext advanceMonth increments month counter", "[simulation][context]")
{
  SimulationContext &ctx = SimulationContext::instance();
  ctx.reset();
  CHECK(ctx.data().month == 0);

  ctx.advanceMonth();
  ctx.advanceMonth();

  CHECK(ctx.data().month == 2);
}
