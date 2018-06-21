// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_view.h"

#include "ash/app_menu/notification_item_view.h"
#include "ash/app_menu/notification_menu_header_view.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

NotificationMenuView::NotificationMenuView(const std::string& app_id)
    : app_id_(app_id) {
  DCHECK(!app_id_.empty())
      << "Only context menus for applications can show notifications.";
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  header_view_ = new NotificationMenuHeaderView();
  AddChildView(header_view_);
}

NotificationMenuView::~NotificationMenuView() = default;

bool NotificationMenuView::IsEmpty() const {
  return notification_item_views_.empty();
}

gfx::Size NotificationMenuView::CalculatePreferredSize() const {
  return gfx::Size(
      views::MenuConfig::instance().touchable_menu_width,
      header_view_->GetPreferredSize().height() + kNotificationItemViewHeight);
}

void NotificationMenuView::AddNotificationItemView(
    const message_center::Notification& notification) {
  // Remove the displayed NotificationItemView, it is still stored in
  // |notification_item_views_|.
  if (!notification_item_views_.empty())
    RemoveChildView(notification_item_views_.front().get());

  notification_item_views_.push_front(std::make_unique<NotificationItemView>(
      notification.title(), notification.message(), notification.icon(),
      notification.id()));
  notification_item_views_.front()->set_owned_by_client();
  AddChildView(notification_item_views_.front().get());

  header_view_->UpdateCounter(notification_item_views_.size());
}

void NotificationMenuView::RemoveNotificationItemView(
    const std::string& notification_id) {
  // Find the view which corresponds to |notification_id|.
  auto notification_iter = std::find_if(
      notification_item_views_.begin(), notification_item_views_.end(),
      [&notification_id](
          const std::unique_ptr<NotificationItemView>& notification_item_view) {
        return notification_item_view->notification_id() == notification_id;
      });
  if (notification_iter == notification_item_views_.end())
    return;

  notification_item_views_.erase(notification_iter);
  header_view_->UpdateCounter(notification_item_views_.size());

  // Replace the displayed view, if it is already being shown this is a no-op.
  if (!notification_item_views_.empty())
    AddChildView(notification_item_views_.front().get());
}

}  // namespace ash
