// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_controller.h"

#include "ash/app_menu/notification_menu_view.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

NotificationMenuController::NotificationMenuController(
    const std::string& app_id,
    views::MenuItemView* root_menu,
    ui::SimpleMenuModel* model)
    : app_id_(app_id),
      root_menu_(root_menu),
      model_(model),
      message_center_observer_(this) {
  message_center_observer_.Add(message_center::MessageCenter::Get());
  InitializeNotificationMenuView();
}

NotificationMenuController::~NotificationMenuController() = default;

void NotificationMenuController::OnNotificationAdded(
    const std::string& notification_id) {
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);

  DCHECK(notification);

  if (notification->notifier_id().id != app_id_)
    return;

  if (!notification_menu_view_) {
    InitializeNotificationMenuView();
    return;
  }

  notification_menu_view_->AddNotificationItemView(*notification);
}

void NotificationMenuController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (!notification_menu_view_)
    return;

  // Remove the view from the container.
  notification_menu_view_->RemoveNotificationItemView(notification_id);

  if (!notification_menu_view_->IsEmpty())
    return;

  // There are no more notifications to show, so remove |item_| from
  // |root_menu_|, and remove the entry from the model.
  const views::View* container = notification_menu_view_->parent();
  root_menu_->RemoveMenuItemAt(root_menu_->GetSubmenu()->GetIndexOf(container));
  // TODO(newcomer): move NOTIFICATION_CONTAINER and other CommandId enums to a
  // shared constant file in ash/public/cpp.
  model_->RemoveItemAt(model_->GetIndexOfCommandId(NOTIFICATION_CONTAINER));
  notification_menu_view_ = nullptr;

  // Notify the root MenuItemView so it knows to resize and re-calculate the
  // menu bounds.
  root_menu_->ChildrenChanged();
}

void NotificationMenuController::InitializeNotificationMenuView() {
  DCHECK(!notification_menu_view_);

  // Initialize the container only if there are notifications to show.
  if (message_center::MessageCenter::Get()
          ->FindNotificationsByAppId(app_id_)
          .empty()) {
    return;
  }

  model_->AddItem(NOTIFICATION_CONTAINER, base::string16());
  // Add the container MenuItemView to |root_menu_|.
  views::MenuItemView* container = root_menu_->AppendMenuItem(
      NOTIFICATION_CONTAINER, base::string16(), views::MenuItemView::NORMAL);
  notification_menu_view_ = new NotificationMenuView(app_id_);
  container->AddChildView(notification_menu_view_);

  for (auto* notification :
       message_center::MessageCenter::Get()->FindNotificationsByAppId(
           app_id_)) {
    notification_menu_view_->AddNotificationItemView(*notification);
  }

  // Notify the root MenuItemView so it knows to resize and re-calculate the
  // menu bounds.
  root_menu_->ChildrenChanged();
}

}  // namespace ash
