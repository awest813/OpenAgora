#ifndef GOVERNANCE_PANEL_HXX_
#define GOVERNANCE_PANEL_HXX_

#include "UIManager.hxx"

/**
 * @class GovernancePanel
 * @brief Persistent governance HUD panel + council checkpoint modal.
 */
struct GovernancePanel : public GameMenu
{
  void draw() const override;
};

#endif // GOVERNANCE_PANEL_HXX_
