#ifndef EVENT_LOG_PANEL_HXX_
#define EVENT_LOG_PANEL_HXX_

#include "UIManager.hxx"

/**
 * @class EventLogPanel
 * @brief Scrollable full governance event history panel.
 */
struct EventLogPanel : public GameMenu
{
  void draw() const override;
};

#endif // EVENT_LOG_PANEL_HXX_
