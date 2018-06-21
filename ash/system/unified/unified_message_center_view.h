// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_MESSAGE_CENTER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_MESSAGE_CENTER_VIEW_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ash/message_center/message_list_view.h"
#include "base/macros.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace message_center {

class MessageCenter;

}  // namespace message_center

namespace ash {

class UnifiedSystemTrayController;

// Container for message list view. Acts as a controller/delegate of message
// list view, passing data back and forth to message center.
class ASH_EXPORT UnifiedMessageCenterView
    : public views::View,
      public message_center::MessageCenterObserver,
      public views::ViewObserver,
      public views::ButtonListener,
      public MessageListView::Observer {
 public:
  UnifiedMessageCenterView(UnifiedSystemTrayController* tray_controller,
                           message_center::MessageCenter* message_center);
  ~UnifiedMessageCenterView() override;

  // Set the maximum height that the view can take.
  void SetMaxHeight(int max_height);

  // Show the animation of clearing all notifications. After the animation is
  // finished, UnifiedSystemTrayController::OnClearAllAnimationEnded() will be
  // called.
  void ShowClearAllAnimation();

 protected:
  void SetNotifications(
      const message_center::NotificationList::Notifications& notifications);

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // MessageListView::Observer:
  void OnAllNotificationsCleared() override;

 private:
  void Update();
  void AddNotificationAt(const message_center::Notification& notification,
                         int index);
  void UpdateNotification(const std::string& notification_id);

  // Scroll the notification list to the bottom.
  void ScrollToBottom();

  UnifiedSystemTrayController* const tray_controller_;
  message_center::MessageCenter* const message_center_;

  views::ScrollView* const scroller_;
  MessageListView* const message_list_view_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_MESSAGE_CENTER_VIEW_H_
