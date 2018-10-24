// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_MENU_CONSTANTS_H_
#define ASH_PUBLIC_CPP_APP_MENU_CONSTANTS_H_

namespace ash {

// NOTIFICATION_CONTAINER is 9 so it matches
// LauncherContextMenu::MenuItem::NOTIFICATION_CONTAINER.
enum CommandId {
  NOTIFICATION_CONTAINER = 9,
};

// Minimum padding for children of NotificationMenuView in dips.
constexpr int kNotificationHorizontalPadding = 16;
constexpr int kNotificationVerticalPadding = 8;

// Height of the NotificationItemView in dips.
constexpr int kNotificationItemViewHeight = 48;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_MENU_CONSTANTS_H_
