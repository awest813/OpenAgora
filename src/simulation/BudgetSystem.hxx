#ifndef BUDGET_SYSTEM_HXX_
#define BUDGET_SYSTEM_HXX_

#include "Singleton.hxx"

#include <deque>

/**
 * @brief Snapshot of a single monthly budget cycle.
 */
struct BudgetSnapshot
{
  int   month          = 0;
  float taxRevenue     = 0.f; ///< Income from building upkeep / tax base
  float policyExpenses = 0.f; ///< Total cost of active policies this month
  float balance        = 0.f; ///< taxRevenue − policyExpenses
};

/**
 * @class BudgetSystem
 * @brief Tracks city income and expenditure each game-month.
 *
 * Pure C++17 – no SDL headers.
 *
 * Each tick:
 *  1. Compute tax revenue from the number of residents/workers on the map,
 *     scaled by the GovernanceSystem approval (high approval → better tax
 *     efficiency).
 *  2. Deduct active policy expenses reported by PolicyEngine.
 *  3. Accumulate running balance.
 *  4. Keep a rolling history of up to MAX_HISTORY snapshots.
 *
 * The UI layer reads currentBalance() and recentHistory() to draw a
 * budget summary.
 */
class BudgetSystem : public Singleton<BudgetSystem>
{
public:
  friend Singleton<BudgetSystem>;

  // ── Configuration ──────────────────────────────────────────────────────────

  struct Config
  {
    float taxRatePerInhabitant = 2.f;   ///< Monthly tax income per inhabitant
    float highApprovalBonus   = 1.10f; ///< Revenue multiplier when approval > 70
    float lowApprovalPenalty  = 0.80f; ///< Revenue multiplier when approval < 30
  };

  void configure(const Config &config);
  void reset();

  /**
   * @brief Advance one game-month.
   * @param totalInhabitants Total inhabitant count summed across all building tiles.
   * @param policyExpenses   Total monthly cost reported by PolicyEngine::tick().
   * @param approval         Current public approval [0–100] from GovernanceSystem.
   */
  void tick(int totalInhabitants, float policyExpenses, float approval);

  float currentBalance()  const { return m_runningBalance; }
  float lastTaxRevenue()  const { return m_lastRevenue; }
  float lastExpenses()    const { return m_lastExpenses; }
  int   monthCounter()    const { return m_month; }

  const std::deque<BudgetSnapshot> &recentHistory() const { return m_history; }

private:
  BudgetSystem() = default;

  static constexpr int MAX_HISTORY = 12;

  Config m_config;
  float  m_runningBalance = 0.f;
  float  m_lastRevenue    = 0.f;
  float  m_lastExpenses   = 0.f;
  int    m_month          = 0;

  std::deque<BudgetSnapshot> m_history;
};

#endif // BUDGET_SYSTEM_HXX_
