// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_controller.h"

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {
namespace test {
namespace {

constexpr char kTestAppId[] = "test-app-id";

void BuildAndSendNotification(const std::string& app_id,
                              const std::string& notification_id) {
  const message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, app_id);
  std::unique_ptr<message_center::Notification> notification =
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          base::ASCIIToUTF16("Test Web Notification"),
          base::ASCIIToUTF16("Notification message body."), gfx::Image(),
          base::ASCIIToUTF16("www.test.org"), GURL(), notifier_id,
          message_center::RichNotificationData(), nullptr /* delegate */);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace

class NotificationMenuControllerTest : public AshTestBase {
 public:
  NotificationMenuControllerTest() = default;
  ~NotificationMenuControllerTest() override {}

  // Overridden from AshTestBase:
  void TearDown() override {
    // NotificationMenuController removes itself from MessageCenter's observer
    // list in the dtor, so force it to happen first to prevent a crash. This
    // crash does not repro in production.
    notification_menu_controller_.reset();
    AshTestBase::TearDown();
  }

  void BuildMenu() {
    model_ = std::make_unique<ui::SimpleMenuModel>(
        nullptr /*ui::SimpleMenuModel::Delegate not required*/);
    model_->AddItem(0, base::ASCIIToUTF16("item 1"));
    model_->AddItem(1, base::ASCIIToUTF16("item 2"));

    delegate_ = std::make_unique<views::MenuModelAdapter>(model_.get());
    root_menu_item_view_ = new views::MenuItemView(delegate_.get());
    host_view_ = std::make_unique<views::View>();
    host_view_->AddChildView(root_menu_item_view_);
    delegate_->BuildMenu(root_menu_item_view_);

    notification_menu_controller_ =
        std::make_unique<NotificationMenuController>(
            kTestAppId, root_menu_item_view_, model_.get());
  }

  views::MenuItemView* root_menu_item_view() { return root_menu_item_view_; }

 private:
  // The root MenuItemView. Owned by |host_view_|.
  views::MenuItemView* root_menu_item_view_ = nullptr;
  // Allows the dtor to access the restricted views::MenuItemView dtor.
  std::unique_ptr<views::View> host_view_;
  std::unique_ptr<NotificationMenuController> notification_menu_controller_;
  std::unique_ptr<ui::SimpleMenuModel> model_;
  std::unique_ptr<views::MenuModelAdapter> delegate_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMenuControllerTest);
};

// Tests that NotificationMenuController does not add the
// NotificationMenuView container until a notification arrives.
TEST_F(NotificationMenuControllerTest, NotificationsArriveAfterBuilt) {
  // Build the context menu without adding a notification for
  // kTestAppId.
  BuildMenu();

  // There should only be two items in the context menu.
  EXPECT_EQ(2, root_menu_item_view()->GetSubmenu()->child_count());

  // Add a notification.
  BuildAndSendNotification(kTestAppId, std::string("notification_id"));

  // NotificationMenuController should have added a third item, the
  // container for NotificationMenuView, to the menu.
  EXPECT_EQ(3, root_menu_item_view()->GetSubmenu()->child_count());
}

// Tests that NotificationMenuController adds and removes the container
// MenuItemView when notifications come in before and after the menu has been
// built.
TEST_F(NotificationMenuControllerTest, NotificationsExistBeforeMenuIsBuilt) {
  // Add the notification before the menu is built.
  const std::string notification_id("notification_id");
  BuildAndSendNotification(kTestAppId, notification_id);

  // Build the menu, the container should be added.
  BuildMenu();
  EXPECT_EQ(3, root_menu_item_view()->GetSubmenu()->child_count());

  // Remove the notification, this should result in the NotificationMenuView
  // container being removed.
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           true);

  EXPECT_EQ(2, root_menu_item_view()->GetSubmenu()->child_count());

  // Add the same notification.
  BuildAndSendNotification(kTestAppId, notification_id);

  // The container MenuItemView should be added again.
  EXPECT_EQ(3, root_menu_item_view()->GetSubmenu()->child_count());
}

// Tests that adding multiple notifications for kTestAppId does not add
// additional containers beyond the first.
TEST_F(NotificationMenuControllerTest, MultipleNotifications) {
  // Add two notifications, then build the menu.
  const std::string notification_id_0("notification_id_0");
  BuildAndSendNotification(kTestAppId, notification_id_0);
  const std::string notification_id_1("notification_id_1");
  BuildAndSendNotification(kTestAppId, notification_id_1);
  BuildMenu();

  // Only one container MenuItemView should be added.
  EXPECT_EQ(3, root_menu_item_view()->GetSubmenu()->child_count());

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  // Remove one of the notifications.
  message_center->RemoveNotification(notification_id_0, true);

  // The container should still exist.
  EXPECT_EQ(3, root_menu_item_view()->GetSubmenu()->child_count());

  // Remove the final notification.
  message_center->RemoveNotification(notification_id_1, true);

  // The container should be removed.
  EXPECT_EQ(2, root_menu_item_view()->GetSubmenu()->child_count());
}

}  // namespace test
}  // namespace ash
