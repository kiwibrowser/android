// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_message_center_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/sign_out_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;
using message_center::NotificationList;

namespace ash {

namespace {

const int kMaxVisibleNotifications = 100;

}  // namespace

// UnifiedMessageCenterView
// ///////////////////////////////////////////////////////////

UnifiedMessageCenterView::UnifiedMessageCenterView(
    UnifiedSystemTrayController* tray_controller,
    MessageCenter* message_center)
    : tray_controller_(tray_controller),
      message_center_(message_center),
      scroller_(new views::ScrollView()),
      message_list_view_(new MessageListView()) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  message_center_->AddObserver(this);
  set_notify_enter_exit_on_child(true);
  SetFocusBehavior(views::View::FocusBehavior::NEVER);

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  scroller_->SetHorizontalScrollBar(new views::OverlayScrollBar(true));
  scroller_->set_draw_overflow_indicator(false);
  AddChildView(scroller_);

  message_list_view_->set_use_fixed_height(false);
  message_list_view_->set_scroller(scroller_);
  message_list_view_->AddObserver(this);

  views::View* scroller_contents = new views::View;
  auto* contents_layout = scroller_contents->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  contents_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  scroller_contents->AddChildView(message_list_view_);

  views::View* button_container = new views::View;
  auto* button_layout =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal,
          gfx::Insets(kUnifiedNotificationCenterSpacing), 0));
  button_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  auto* clear_all_button = new RoundedLabelButton(
      this, l10n_util::GetStringUTF16(
                IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));
  button_container->AddChildView(clear_all_button);
  scroller_contents->AddChildView(button_container);

  scroller_->SetContents(scroller_contents);

  SetNotifications(message_center_->GetVisibleNotifications());
}

UnifiedMessageCenterView::~UnifiedMessageCenterView() {
  message_center_->RemoveObserver(this);
}

void UnifiedMessageCenterView::SetMaxHeight(int max_height) {
  scroller_->ClipHeightTo(0, max_height);
  Update();
}

void UnifiedMessageCenterView::ShowClearAllAnimation() {
  message_list_view_->ClearAllClosableNotifications(
      scroller_->GetVisibleRect());
}

void UnifiedMessageCenterView::SetNotifications(
    const NotificationList::Notifications& notifications) {
  int index = message_list_view_->GetNotificationCount();
  for (const Notification* notification : notifications) {
    if (index >= kMaxVisibleNotifications) {
      break;
    }

    AddNotificationAt(*notification, 0);
    message_center_->DisplayedNotification(
        notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    index++;
  }

  Update();
}

void UnifiedMessageCenterView::Layout() {
  scroller_->SetBounds(0, 0, width(), height());
  ScrollToBottom();
}

gfx::Size UnifiedMessageCenterView::CalculatePreferredSize() const {
  gfx::Size preferred_size = scroller_->GetPreferredSize();
  // Hide Clear All button at the buttom from initial viewport.
  preferred_size.set_height(preferred_size.height() -
                            3 * kUnifiedNotificationCenterSpacing);
  return preferred_size;
}

void UnifiedMessageCenterView::OnNotificationAdded(const std::string& id) {
  if (message_list_view_->GetNotificationCount() >= kMaxVisibleNotifications)
    return;

  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  for (const Notification* notification : notifications) {
    if (notification->id() == id) {
      AddNotificationAt(*notification,
                        message_list_view_->GetNotificationCount());
      break;
    }
  }
  Update();
  ScrollToBottom();
}

void UnifiedMessageCenterView::OnNotificationRemoved(const std::string& id,
                                                     bool by_user) {
  auto view_pair = message_list_view_->GetNotificationById(id);
  MessageView* view = view_pair.second;
  if (!view)
    return;

  message_list_view_->RemoveNotification(view);
  Update();
}

void UnifiedMessageCenterView::OnNotificationUpdated(const std::string& id) {
  UpdateNotification(id);
}

void UnifiedMessageCenterView::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  DCHECK_EQ(std::string(MessageView::kViewClassName),
            observed_view->GetClassName());
  UpdateNotification(
      static_cast<MessageView*>(observed_view)->notification_id());
}

void UnifiedMessageCenterView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  tray_controller_->HandleClearAllAction();
}

void UnifiedMessageCenterView::OnAllNotificationsCleared() {
  tray_controller_->OnClearAllAnimationEnded();
}

void UnifiedMessageCenterView::Update() {
  SetVisible(message_list_view_->GetNotificationCount() > 0);

  size_t notification_count = message_list_view_->GetNotificationCount();
  // TODO(tetsui): This is O(n^2).
  for (size_t i = 0; i < notification_count; ++i) {
    auto* view = message_list_view_->GetNotificationAt(i);
    const int top_radius =
        i == notification_count - 1 ? kUnifiedTrayCornerRadius : 0;
    const int bottom_radius = i == 0 ? kUnifiedTrayCornerRadius : 0;
    view->UpdateCornerRadius(top_radius, bottom_radius);
    bool has_bottom_separator = i > 0 && notification_count > 1;
    view->SetBorder(views::CreateSolidSidedBorder(
        0, 0, has_bottom_separator ? kUnifiedNotificationSeparatorThickness : 0,
        0, kUnifiedNotificationSeparatorColor));
  }

  scroller_->Layout();
  PreferredSizeChanged();
}

void UnifiedMessageCenterView::AddNotificationAt(
    const Notification& notification,
    int index) {
  MessageView* view = message_center::MessageViewFactory::Create(
      notification, /*top-level=*/false);
  view->AddObserver(this);
  view->set_scroller(scroller_);
  message_list_view_->AddNotificationAt(view, index);
}

void UnifiedMessageCenterView::UpdateNotification(const std::string& id) {
  MessageView* view = message_list_view_->GetNotificationById(id).second;
  if (!view)
    return;

  Notification* notification = message_center_->FindVisibleNotificationById(id);
  if (!notification)
    return;

  int old_width = view->width();
  int old_height = view->height();
  bool old_pinned = view->GetPinned();
  message_list_view_->UpdateNotification(view, *notification);
  if (view->GetHeightForWidth(old_width) != old_height ||
      view->GetPinned() != old_pinned) {
    Update();
  }
}

void UnifiedMessageCenterView::ScrollToBottom() {
  // Hide Clear All button at the buttom from initial viewport.
  int max_position_without_button =
      scroller_->vertical_scroll_bar()->GetMaxPosition() -
      3 * kUnifiedNotificationCenterSpacing;
  scroller_->ScrollToPosition(
      const_cast<views::ScrollBar*>(scroller_->vertical_scroll_bar()),
      max_position_without_button);
}

}  // namespace ash
