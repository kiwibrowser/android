// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_view.h"

#include "ash/app_menu/notification_item_view.h"
#include "ash/app_menu/notification_menu_view_test_api.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace test {

namespace {

// The app id used in tests.
constexpr char kTestAppId[] = "test-app-id";

}  // namespace

class NotificationMenuViewTest : public views::ViewsTestBase {
 public:
  NotificationMenuViewTest() {}
  ~NotificationMenuViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    notification_menu_view_ =
        std::make_unique<NotificationMenuView>(kTestAppId);
    test_api_ = std::make_unique<NotificationMenuViewTestAPI>(
        notification_menu_view_.get());
  }

  message_center::Notification AddNotification(
      const std::string& notification_id,
      const base::string16& title,
      const base::string16& message) {
    const message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION, kTestAppId);
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
        message, gfx::Image(), base::ASCIIToUTF16("www.test.org"), GURL(),
        notifier_id, message_center::RichNotificationData(),
        nullptr /* delegate */);
    notification_menu_view_->AddNotificationItemView(notification);
    return notification;
  }

  void CheckDisplayedNotification(
      const message_center::Notification& notification) {
    // Check whether the notification and view contents match.
    NotificationItemView* item_view =
        test_api_->GetDisplayedNotificationItemView();
    ASSERT_TRUE(item_view);
    EXPECT_EQ(item_view->notification_id(), notification.id());
    EXPECT_EQ(item_view->title(), notification.title());
    EXPECT_EQ(item_view->message(), notification.message());
  }

  NotificationMenuView* notification_menu_view() {
    return notification_menu_view_.get();
  }

  NotificationMenuViewTestAPI* test_api() { return test_api_.get(); }

 private:
  std::unique_ptr<NotificationMenuView> notification_menu_view_;
  std::unique_ptr<NotificationMenuViewTestAPI> test_api_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMenuViewTest);
};

// Tests that the correct NotificationItemView is shown when notifications come
// and go.
TEST_F(NotificationMenuViewTest, Basic) {
  // Add a notification to the view.
  const message_center::Notification notification_0 =
      AddNotification("notification_id_0", base::ASCIIToUTF16("title_0"),
                      base::ASCIIToUTF16("message_0"));

  // The counter should update to 1, and the displayed NotificationItemView
  // should match the notification.
  EXPECT_EQ(base::IntToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_0);

  // Add a second notification to the view, the counter view and displayed
  // NotificationItemView should change.
  const message_center::Notification notification_1 =
      AddNotification("notification_id_1", base::ASCIIToUTF16("title_1"),
                      base::ASCIIToUTF16("message_1"));
  EXPECT_EQ(base::IntToString16(2), test_api()->GetCounterViewContents());
  EXPECT_EQ(2, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_1);

  // Remove |notification_1|, |notification_0| should be shown.
  notification_menu_view()->RemoveNotificationItemView(notification_1.id());
  EXPECT_EQ(base::IntToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_0);
}

// Tests that removing a notification that is not being shown only updates the
// counter.
TEST_F(NotificationMenuViewTest, RemoveOlderNotification) {
  // Add two notifications.
  const message_center::Notification notification_0 =
      AddNotification("notification_id_0", base::ASCIIToUTF16("title_0"),
                      base::ASCIIToUTF16("message_0"));
  const message_center::Notification notification_1 =
      AddNotification("notification_id_1", base::ASCIIToUTF16("title_1"),
                      base::ASCIIToUTF16("message_1"));

  // The latest notification should be shown.
  EXPECT_EQ(base::IntToString16(2), test_api()->GetCounterViewContents());
  EXPECT_EQ(2, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_1);

  // Remove the older notification, |notification_0|.
  notification_menu_view()->RemoveNotificationItemView(notification_0.id());

  // The latest notification, |notification_1|, should be shown.
  EXPECT_EQ(base::IntToString16(1), test_api()->GetCounterViewContents());
  EXPECT_EQ(1, test_api()->GetItemViewCount());
  CheckDisplayedNotification(notification_1);
}

}  // namespace test
}  // namespace ash
