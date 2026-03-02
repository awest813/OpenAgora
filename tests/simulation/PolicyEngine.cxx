#include <catch.hpp>

#include "../../src/simulation/PolicyEngine.hxx"

// ── Helpers ─────────────────────────────────────────────────────────────────

static PolicyDefinition makePolicy(const std::string &id, int costPerMonth,
                                   const std::vector<PolicyEffect> &effects)
{
  PolicyDefinition def;
  def.id           = id;
  def.label        = id;
  def.description  = "Test policy";
  def.type         = "budget_sink";
  def.costPerMonth = costPerMonth;
  def.effects      = effects;
  return def;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("PolicyEngine starts empty after clearPolicies", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();
  CHECK(pe.definitions().empty());
}

TEST_CASE("Inactive policy applies no effects", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  PolicyEffect eff;
  eff.target = "affordabilityIndex";
  eff.op     = "add";
  eff.value  = 20.f;

  pe.addDefinition(makePolicy("test_inactive", 500, {eff}));

  AffordabilityState aff{};
  aff.affordabilityIndex = 40.f;
  CityIndicesData indices{};

  const int cost = pe.tick(aff, indices);
  CHECK(cost == 0);
  CHECK(aff.affordabilityIndex == Approx(40.f)); // unchanged
}

TEST_CASE("Active policy with add effect modifies affordability", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  PolicyEffect eff;
  eff.target = "affordabilityIndex";
  eff.op     = "add";
  eff.value  = 8.f;

  pe.addDefinition(makePolicy("housing_fund", 500, {eff}));
  pe.setActive("housing_fund", true);

  AffordabilityState aff{};
  aff.affordabilityIndex = 40.f;
  CityIndicesData indices{};

  const int cost = pe.tick(aff, indices);
  CHECK(cost == 500);
  CHECK(aff.affordabilityIndex == Approx(48.f));
}

TEST_CASE("Active policy with add effect modifies displacement pressure", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  PolicyEffect eff;
  eff.target = "displacementPressure";
  eff.op     = "add";
  eff.value  = -5.f;

  pe.addDefinition(makePolicy("rent_relief", 300, {eff}));
  pe.setActive("rent_relief", true);

  AffordabilityState aff{};
  aff.displacementPressure = 30.f;
  CityIndicesData indices{};

  pe.tick(aff, indices);
  CHECK(aff.displacementPressure == Approx(25.f));
}

TEST_CASE("Active policy modifies city indices", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  PolicyEffect eff;
  eff.target = "safetyIndex";
  eff.op     = "add";
  eff.value  = 10.f;

  pe.addDefinition(makePolicy("safety_campaign", 200, {eff}));
  pe.setActive("safety_campaign", true);

  AffordabilityState aff{};
  CityIndicesData indices{};
  indices.safety = 50.f;

  pe.tick(aff, indices);
  CHECK(indices.safety == Approx(60.f));
}

TEST_CASE("Policy effects are clamped to [0, 100]", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  PolicyEffect eff1;
  eff1.target = "affordabilityIndex";
  eff1.op     = "add";
  eff1.value  = 200.f; // would overflow

  PolicyEffect eff2;
  eff2.target = "displacementPressure";
  eff2.op     = "add";
  eff2.value  = -500.f; // would underflow

  pe.addDefinition(makePolicy("overflow_policy", 0, {eff1, eff2}));
  pe.setActive("overflow_policy", true);

  AffordabilityState aff{};
  aff.affordabilityIndex   = 90.f;
  aff.displacementPressure = 10.f;
  CityIndicesData indices{};

  pe.tick(aff, indices);
  CHECK(aff.affordabilityIndex   == Approx(100.f));
  CHECK(aff.displacementPressure == Approx(0.f));
}

TEST_CASE("Multiple policies accumulate cost and effects", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  PolicyEffect eff1;
  eff1.target = "affordabilityIndex";
  eff1.op     = "add";
  eff1.value  = 5.f;

  PolicyEffect eff2;
  eff2.target = "affordabilityIndex";
  eff2.op     = "add";
  eff2.value  = 3.f;

  pe.addDefinition(makePolicy("policy_a", 400, {eff1}));
  pe.addDefinition(makePolicy("policy_b", 600, {eff2}));
  pe.setActive("policy_a", true);
  pe.setActive("policy_b", true);

  AffordabilityState aff{};
  aff.affordabilityIndex = 50.f;
  CityIndicesData indices{};

  const int cost = pe.tick(aff, indices);
  CHECK(cost == 1000);
  CHECK(aff.affordabilityIndex == Approx(58.f));
}

TEST_CASE("setActive returns false for unknown policy id", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();
  CHECK_FALSE(pe.setActive("nonexistent_policy", true));
}

TEST_CASE("isActive reflects setActive calls", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  pe.addDefinition(makePolicy("toggle_me", 0, {}));
  CHECK_FALSE(pe.isActive("toggle_me"));

  pe.setActive("toggle_me", true);
  CHECK(pe.isActive("toggle_me"));

  pe.setActive("toggle_me", false);
  CHECK_FALSE(pe.isActive("toggle_me"));
}

TEST_CASE("Multiply op scales target value", "[simulation][policy]")
{
  PolicyEngine &pe = PolicyEngine::instance();
  pe.clearPolicies();

  PolicyEffect eff;
  eff.target = "medianRent";
  eff.op     = "multiply";
  eff.value  = 0.8f;

  pe.addDefinition(makePolicy("rent_cap", 0, {eff}));
  pe.setActive("rent_cap", true);

  AffordabilityState aff{};
  aff.medianRent = 60.f;
  CityIndicesData indices{};

  pe.tick(aff, indices);
  CHECK(aff.medianRent == Approx(48.f));
}
