#include <catch.hpp>

#include "../../src/simulation/PolicyEngine.hxx"

namespace
{
PolicyDefinition makeBasePolicy(const std::string &id)
{
  PolicyDefinition def;
  def.id = id;
  def.label = id;
  def.description = "test";
  def.category = "test";
  def.type = "budget_sink";
  return def;
}
} // namespace

TEST_CASE("PolicyEngine level selection and costs work", "[simulation][policy][policy_v2]")
{
  PolicyEngine &engine = PolicyEngine::instance();
  engine.clearPolicies();

  PolicyDefinition def = makeBasePolicy("leveled_policy");
  def.levels = {
      PolicyLevelDefinition{1, 200, 0.f, {PolicyEffect{"affordabilityIndex", "add", 5.f}}},
      PolicyLevelDefinition{2, 400, 0.f, {PolicyEffect{"affordabilityIndex", "add", 10.f}}},
  };
  engine.addDefinition(def);

  REQUIRE(engine.setPolicyLevel("leveled_policy", 2));
  CHECK(engine.policyLevel("leveled_policy") == 2);

  AffordabilityState aff{};
  aff.affordabilityIndex = 40.f;
  CityIndicesData idx{};
  SimulationContextData ctx{};

  const int cost = engine.tick(aff, idx, &ctx);
  CHECK(cost == 400);
  CHECK(aff.affordabilityIndex == Approx(50.f));
}

TEST_CASE("PolicyEngine availability enforces prerequisites and exclusivity", "[simulation][policy][policy_v2]")
{
  PolicyEngine &engine = PolicyEngine::instance();
  engine.clearPolicies();

  PolicyDefinition base = makeBasePolicy("base");
  base.levels = {PolicyLevelDefinition{1, 100, 0.f, {}}};
  engine.addDefinition(base);

  PolicyDefinition dependent = makeBasePolicy("dependent");
  dependent.requires = {"base"};
  dependent.exclusiveWith = {"mutual"};
  dependent.levels = {PolicyLevelDefinition{1, 100, 0.f, {}}};
  engine.addDefinition(dependent);

  PolicyDefinition mutual = makeBasePolicy("mutual");
  mutual.levels = {PolicyLevelDefinition{1, 100, 0.f, {}}};
  engine.addDefinition(mutual);

  auto unavailablePrereq = engine.availability("dependent", 1, 80.f);
  CHECK_FALSE(unavailablePrereq.available);

  REQUIRE(engine.setPolicyLevel("base", 1));
  auto nowAvailable = engine.availability("dependent", 1, 80.f);
  CHECK(nowAvailable.available);

  REQUIRE(engine.setPolicyLevel("mutual", 1));
  auto unavailableExclusive = engine.availability("dependent", 1, 80.f);
  CHECK_FALSE(unavailableExclusive.available);
}

TEST_CASE("PolicyEngine availability enforces approval gates", "[simulation][policy][policy_v2]")
{
  PolicyEngine &engine = PolicyEngine::instance();
  engine.clearPolicies();

  PolicyDefinition def = makeBasePolicy("approval_gate");
  def.minApproval = 40.f;
  def.levels = {
      PolicyLevelDefinition{1, 150, 0.f, {}},
      PolicyLevelDefinition{2, 250, 65.f, {}},
  };
  engine.addDefinition(def);

  auto lowForL1 = engine.availability("approval_gate", 1, 30.f);
  CHECK_FALSE(lowForL1.available);

  auto enoughForL1 = engine.availability("approval_gate", 1, 45.f);
  CHECK(enoughForL1.available);

  auto lowForL2 = engine.availability("approval_gate", 2, 60.f);
  CHECK_FALSE(lowForL2.available);

  auto enoughForL2 = engine.availability("approval_gate", 2, 70.f);
  CHECK(enoughForL2.available);
}

TEST_CASE("Temporary policy auto-expires after duration", "[simulation][policy][policy_v2]")
{
  PolicyEngine &engine = PolicyEngine::instance();
  engine.clearPolicies();

  PolicyDefinition def = makeBasePolicy("temporary");
  def.durationMonths = 2;
  def.levels = {PolicyLevelDefinition{1, 100, 0.f, {PolicyEffect{"affordabilityIndex", "add", 1.f}}}};
  engine.addDefinition(def);

  REQUIRE(engine.setPolicyLevel("temporary", 1));

  AffordabilityState aff{};
  CityIndicesData idx{};
  SimulationContextData ctx{};

  CHECK(engine.isActive("temporary"));
  engine.tick(aff, idx, &ctx); // month 1
  CHECK(engine.isActive("temporary"));
  engine.tick(aff, idx, &ctx); // month 2 expires
  CHECK_FALSE(engine.isActive("temporary"));
}

TEST_CASE("PolicyEngine can apply effects to simulation context fields", "[simulation][policy][policy_v2]")
{
  PolicyEngine &engine = PolicyEngine::instance();
  engine.clearPolicies();

  PolicyDefinition def = makeBasePolicy("context_target");
  def.levels = {PolicyLevelDefinition{
      1,
      100,
      0.f,
      {
          PolicyEffect{"taxEfficiency", "add", 0.2f},
          PolicyEffect{"growthRateModifier", "multiply", 1.1f},
      }}};
  engine.addDefinition(def);
  REQUIRE(engine.setPolicyLevel("context_target", 1));

  AffordabilityState aff{};
  CityIndicesData idx{};
  SimulationContextData ctx{};

  engine.tick(aff, idx, &ctx);
  CHECK(ctx.taxEfficiency == Approx(1.2f));
  CHECK(ctx.growthRateModifier == Approx(1.1f));
}

