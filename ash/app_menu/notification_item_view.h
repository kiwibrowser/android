// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_

#include <string>

#include "ash/app_menu/app_menu_export.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace gfx {
class Image;
class Size;
}  // namespace gfx

namespace message_center {
class ProportionalImageView;
}

namespace ash {

// The view which contains the details of a notification.
class APP_MENU_EXPORT NotificationItemView : public views::View {
 public:
  NotificationItemView(const base::string16& title,
                       const base::string16& message,
                       const gfx::Image& icon,
                       const std::string notification_id);

  ~NotificationItemView() override;

  // views::View overrides:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  const std::string& notification_id() const { return notification_id_; }
  const base::string16& title() const { return title_; }
  const base::string16& message() const { return message_; }

 private:
  // Holds the title and message labels. Owned by the views hierarchy.
  views::View* text_container_ = nullptr;

  // Holds the notification's icon. Owned by the views hierarchy.
  message_center::ProportionalImageView* proportional_icon_view_ = nullptr;

  // Notification properties.
  const base::string16 title_;
  const base::string16 message_;

  // The identifier used by MessageCenter to identify this notification.
  const std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(NotificationItemView);
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_
