#ifndef NOTIFICATION_OVERLAY_HXX_
#define NOTIFICATION_OVERLAY_HXX_

#include "UIManager.hxx"

/**
 * @class NotificationOverlay
 * @brief Bottom-right toast overlay for governance event notifications.
 *
 * Polls GovernanceSystem::recentNotifications() each frame and displays the
 * most recent unseen notification as a timed toast (auto-dismisses after
 * TOAST_DURATION_SEC seconds of real time).
 */
struct NotificationOverlay : public GameMenu
{
  void draw() const override;
};

#endif // NOTIFICATION_OVERLAY_HXX_
