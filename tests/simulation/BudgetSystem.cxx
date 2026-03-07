#include <catch.hpp>

#include "../../src/simulation/BudgetSystem.hxx"

TEST_CASE("BudgetSystem starts at zero after reset", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  budget.reset();

  CHECK(budget.currentBalance() == Approx(0.f));
  CHECK(budget.lastTaxRevenue() == Approx(0.f));
  CHECK(budget.lastExpenses()   == Approx(0.f));
  CHECK(budget.monthCounter()   == 0);
  CHECK(budget.recentHistory().empty());
}

TEST_CASE("Tax revenue grows with inhabitant count", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  BudgetSystem::Config cfg;
  cfg.taxRatePerInhabitant = 2.f;
  cfg.highApprovalBonus    = 1.10f;
  cfg.lowApprovalPenalty   = 0.80f;
  budget.configure(cfg);
  budget.reset();

  budget.tick(100, 0.f, 50.f);
  CHECK(budget.lastTaxRevenue() == Approx(200.f));

  budget.reset();
  budget.tick(200, 0.f, 50.f);
  CHECK(budget.lastTaxRevenue() == Approx(400.f));
}

TEST_CASE("High approval gives revenue bonus", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  BudgetSystem::Config cfg;
  cfg.taxRatePerInhabitant = 2.f;
  cfg.highApprovalBonus    = 1.10f;
  cfg.lowApprovalPenalty   = 0.80f;
  budget.configure(cfg);
  budget.reset();

  budget.tick(100, 0.f, 80.f); // approval > 70
  const float highRevenue = budget.lastTaxRevenue();

  budget.reset();
  budget.tick(100, 0.f, 50.f); // neutral approval
  const float neutralRevenue = budget.lastTaxRevenue();

  CHECK(highRevenue > neutralRevenue);
  CHECK(highRevenue == Approx(220.f)); // 100 * 2 * 1.10
}

TEST_CASE("Low approval applies revenue penalty", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  BudgetSystem::Config cfg;
  cfg.taxRatePerInhabitant = 2.f;
  cfg.highApprovalBonus    = 1.10f;
  cfg.lowApprovalPenalty   = 0.80f;
  budget.configure(cfg);
  budget.reset();

  budget.tick(100, 0.f, 20.f); // approval < 30
  const float lowRevenue = budget.lastTaxRevenue();

  CHECK(lowRevenue == Approx(160.f)); // 100 * 2 * 0.80
}

TEST_CASE("Policy expenses reduce running balance", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  budget.reset();

  budget.tick(100, 300.f, 50.f); // revenue 200, expenses 300 → balance -100
  CHECK(budget.currentBalance() == Approx(-100.f));
  CHECK(budget.lastExpenses()   == Approx(300.f));
}

TEST_CASE("Running balance accumulates across ticks", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  budget.reset();

  BudgetSystem::Config cfg;
  cfg.taxRatePerInhabitant = 10.f;
  cfg.highApprovalBonus    = 1.0f;
  cfg.lowApprovalPenalty   = 1.0f;
  budget.configure(cfg);
  budget.reset();

  budget.tick(50, 0.f, 50.f); // +500
  budget.tick(50, 0.f, 50.f); // +500
  CHECK(budget.currentBalance() == Approx(1000.f));
  CHECK(budget.monthCounter()   == 2);
}

TEST_CASE("History rolls over at MAX_HISTORY months", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  budget.reset();

  // Tick 13 times (MAX_HISTORY = 12)
  for (int i = 0; i < 13; ++i)
    budget.tick(10, 0.f, 50.f);

  CHECK(budget.recentHistory().size() == 12);
  CHECK(budget.recentHistory().front().month == 2); // oldest retained is month 2
}

TEST_CASE("BudgetSystem persisted state round-trips", "[simulation][budget]")
{
  BudgetSystem &budget = BudgetSystem::instance();
  budget.reset();

  budget.tick(150, 50.f, 60.f);
  budget.tick(200, 40.f, 65.f);
  const BudgetPersistedState persisted = budget.persistedState();

  budget.reset();
  budget.applyPersistedState(persisted);

  CHECK(budget.currentBalance() == Approx(persisted.runningBalance));
  CHECK(budget.lastTaxRevenue() == Approx(persisted.lastRevenue));
  CHECK(budget.lastExpenses() == Approx(persisted.lastExpenses));
  CHECK(budget.monthCounter() == persisted.month);
}
