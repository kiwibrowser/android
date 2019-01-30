// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_

#include <deque>
#include <string>

#include "ash/app_menu/app_menu_export.h"
#include "ui/views/view.h"

namespace message_center {
class Notification;
}

namespace ash {

class NotificationMenuHeaderView;
class NotificationItemView;

// A view inserted into a container MenuItemView which shows a
// NotificationItemView and a NotificationMenuHeaderView.
class APP_MENU_EXPORT NotificationMenuView : public views::View {
 public:
  explicit NotificationMenuView(const std::string& app_id);
  ~NotificationMenuView() override;

  // Whether |notifications_for_this_app_| is empty.
  bool IsEmpty() const;

  // Adds |notification| as a NotificationItemView, displacing the currently
  // displayed NotificationItemView, if it exists.
  void AddNotificationItemView(
      const message_center::Notification& notification);

  // Removes the NotificationItemView associated with |notification_id| and
  // if it is the currently displayed NotificationItemView, replaces it with the
  // next one if available.
  void RemoveNotificationItemView(const std::string& notification_id);

  // views::View overrides:
  gfx::Size CalculatePreferredSize() const override;

 private:
  friend class NotificationMenuViewTestAPI;

  // Identifies the app for this menu.
  const std::string app_id_;

  // The deque of NotificationItemViews. The front item in the deque is the view
  // which is shown.
  std::deque<std::unique_ptr<NotificationItemView>> notification_item_views_;

  // Holds the header and counter texts. Owned by this.
  NotificationMenuHeaderView* header_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NotificationMenuView);
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_
