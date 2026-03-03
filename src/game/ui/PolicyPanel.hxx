#ifndef POLICY_PANEL_HXX_
#define POLICY_PANEL_HXX_

#include "UIManager.hxx"

/**
 * @class PolicyPanel
 * @brief Persistent policy management panel.
 *
 * Shows all loaded policies from PolicyEngine with per-policy toggles,
 * descriptions, monthly costs, and the current BudgetSystem balance.
 */
struct PolicyPanel : public GameMenu
{
  void draw() const override;
};

#endif // POLICY_PANEL_HXX_
