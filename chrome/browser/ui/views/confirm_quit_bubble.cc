// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_quit_bubble.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/subtle_notification_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kSlideDuration =
    base::TimeDelta::FromMilliseconds(200);

}  // namespace

ConfirmQuitBubble::ConfirmQuitBubble()
    : animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  animation_->SetSlideDuration(kSlideDuration.InMilliseconds());
}

ConfirmQuitBubble::~ConfirmQuitBubble() {}

void ConfirmQuitBubble::Show() {
  animation_->Show();
}

void ConfirmQuitBubble::Hide() {
  animation_->Hide();
}

void ConfirmQuitBubble::AnimationProgressed(const gfx::Animation* animation) {
  float opacity = static_cast<float>(animation->CurrentValueBetween(0.0, 1.0));
  if (opacity == 0) {
    popup_.reset();
  } else {
    if (!popup_) {
      SubtleNotificationView* view = new SubtleNotificationView();

      popup_ = std::make_unique<views::Widget>();
      views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);

      // Set the bounds to that of the active browser window so that the widget
      // will be centered on the nearest monitor.
      params.bounds = BrowserView::GetBrowserViewForBrowser(
                          BrowserList::GetInstance()->GetLastActive())
                          ->GetBounds();
      params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
      params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
      params.accept_events = false;
      params.keep_on_top = true;
      popup_->Init(params);
      popup_->SetContentsView(view);

      view->UpdateContent(l10n_util::GetStringFUTF16(
          IDS_CONFIRM_TO_QUIT_DESCRIPTION,
          l10n_util::GetStringUTF16(IDS_APP_CTRL_KEY),
          l10n_util::GetStringUTF16(IDS_APP_SHIFT_KEY),
          ui::Accelerator(ui::VKEY_Q, 0).GetShortcutText()));

      popup_->CenterWindow(view->GetPreferredSize());

      popup_->ShowInactive();
    }
    popup_->SetOpacity(opacity);
  }
}

void ConfirmQuitBubble::AnimationEnded(const gfx::Animation* animation) {
  AnimationProgressed(animation);
}
