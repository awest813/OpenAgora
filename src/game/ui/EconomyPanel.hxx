#ifndef ECONOMY_PANEL_HXX_
#define ECONOMY_PANEL_HXX_

#include "UIManager.hxx"

/**
 * @class EconomyPanel
 * @brief Persistent panel for deeper economy/service indicators.
 */
struct EconomyPanel : public GameMenu
{
  void draw() const override;
};

#endif // ECONOMY_PANEL_HXX_
