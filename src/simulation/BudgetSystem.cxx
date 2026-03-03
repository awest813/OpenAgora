#include "BudgetSystem.hxx"

void BudgetSystem::configure(const Config &config)
{
  m_config = config;
}

void BudgetSystem::reset()
{
  m_runningBalance = 0.f;
  m_lastRevenue    = 0.f;
  m_lastExpenses   = 0.f;
  m_month          = 0;
  m_history.clear();
}

void BudgetSystem::tick(int totalInhabitants, float policyExpenses, float approval)
{
  ++m_month;

  // ── Tax revenue ────────────────────────────────────────────────────────────
  float efficiencyMultiplier = 1.f;
  if (approval > 70.f)
    efficiencyMultiplier = m_config.highApprovalBonus;
  else if (approval < 30.f)
    efficiencyMultiplier = m_config.lowApprovalPenalty;

  m_lastRevenue  = static_cast<float>(totalInhabitants) * m_config.taxRatePerInhabitant * efficiencyMultiplier;
  m_lastExpenses = policyExpenses;

  const float monthlyBalance = m_lastRevenue - m_lastExpenses;
  m_runningBalance += monthlyBalance;

  // ── History ────────────────────────────────────────────────────────────────
  BudgetSnapshot snap;
  snap.month          = m_month;
  snap.taxRevenue     = m_lastRevenue;
  snap.policyExpenses = m_lastExpenses;
  snap.balance        = monthlyBalance;
  m_history.push_back(snap);

  if (static_cast<int>(m_history.size()) > MAX_HISTORY)
    m_history.pop_front();
}
