// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_quit_bubble_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/confirm_quit.h"
#include "chrome/browser/ui/views/confirm_quit_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"

namespace {

constexpr ui::KeyboardCode kAcceleratorKeyCode = ui::VKEY_Q;
constexpr int kAcceleratorModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;

}  // namespace

// static
ConfirmQuitBubbleController* ConfirmQuitBubbleController::GetInstance() {
  return base::Singleton<ConfirmQuitBubbleController>::get();
}

ConfirmQuitBubbleController::ConfirmQuitBubbleController()
    : ConfirmQuitBubbleController(std::make_unique<ConfirmQuitBubble>(),
                                  std::make_unique<base::OneShotTimer>(),
                                  std::make_unique<gfx::SlideAnimation>(this)) {
}

ConfirmQuitBubbleController::ConfirmQuitBubbleController(
    std::unique_ptr<ConfirmQuitBubbleBase> bubble,
    std::unique_ptr<base::Timer> hide_timer,
    std::unique_ptr<gfx::SlideAnimation> animation)
    : view_(std::move(bubble)),
      state_(State::kWaiting),
      hide_timer_(std::move(hide_timer)),
      browser_hide_animation_(std::move(animation)) {
  browser_hide_animation_->SetSlideDuration(
      confirm_quit::kWindowFadeOutDuration.InMilliseconds());
  BrowserList::AddObserver(this);
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

ConfirmQuitBubbleController::~ConfirmQuitBubbleController() {
  BrowserList::RemoveObserver(this);
}

void ConfirmQuitBubbleController::OnKeyEvent(ui::KeyEvent* event) {
  const ui::Accelerator accelerator(*event);
  if (state_ == State::kQuitting)
    return;
  if (accelerator.key_code() == kAcceleratorKeyCode &&
      accelerator.modifiers() == kAcceleratorModifiers &&
      accelerator.key_state() == ui::Accelerator::KeyState::PRESSED &&
      !accelerator.IsRepeat()) {
    if (state_ == State::kWaiting) {
      Browser* browser = BrowserList::GetInstance()->GetLastActive();
      PrefService* prefs = browser ? browser->profile()->GetPrefs() : nullptr;
      if (!IsFeatureEnabled() ||
          (prefs && !prefs->GetBoolean(prefs::kConfirmToQuitEnabled))) {
        confirm_quit::RecordHistogram(confirm_quit::kNoConfirm);
        Quit();
        event->SetHandled();
        return;
      }
      if (browser) {
        browser_ = browser;
        pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
        pref_change_registrar_->Init(prefs);
        pref_change_registrar_->Add(
            prefs::kConfirmToQuitEnabled,
            base::BindRepeating(
                &ConfirmQuitBubbleController::OnConfirmToQuitPrefChanged,
                base::Unretained(this)));
      }
      state_ = State::kPressed;
      view_->Show();
      hide_timer_->Start(FROM_HERE, confirm_quit::kShowDuration, this,
                         &ConfirmQuitBubbleController::OnTimerElapsed);
      event->SetHandled();
    } else if (state_ == State::kReleased) {
      // The accelerator was pressed while the bubble was showing.  Consider
      // this a confirmation to quit.
      second_press_start_time_ = accelerator.time_stamp();
      ConfirmQuit();
      event->SetHandled();
    }
  } else if (accelerator.key_code() == kAcceleratorKeyCode &&
             accelerator.key_state() == ui::Accelerator::KeyState::RELEASED) {
    if (state_ == State::kPressed) {
      state_ = State::kReleased;
    } else if (state_ == State::kConfirmed) {
      if (!second_press_start_time_.is_null()) {
        if (accelerator.time_stamp() - second_press_start_time_ <
            confirm_quit::kDoubleTapTimeDelta) {
          confirm_quit::RecordHistogram(confirm_quit::kDoubleTap);
        } else {
          confirm_quit::RecordHistogram(confirm_quit::kTapHold);
        }
      }
      Quit();
    }
    event->SetHandled();
  }
}

void ConfirmQuitBubbleController::OnBrowserRemoved(Browser* browser) {
  // A browser is definitely no longer active if it is removed.
  OnBrowserNoLongerActive(browser);
}

void ConfirmQuitBubbleController::OnBrowserNoLongerActive(Browser* browser) {
  if (browser == browser_)
    Reset();
}

void ConfirmQuitBubbleController::DoQuit() {
  chrome::Exit();
}

bool ConfirmQuitBubbleController::IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kWarnBeforeQuitting);
}

void ConfirmQuitBubbleController::AnimationProgressed(
    const gfx::Animation* animation) {
  float opacity = static_cast<float>(animation->CurrentValueBetween(1.0, 0.0));
  for (Browser* browser : *BrowserList::GetInstance()) {
    BrowserView::GetBrowserViewForBrowser(browser)->GetWidget()->SetOpacity(
        opacity);
  }
}

void ConfirmQuitBubbleController::AnimationEnded(
    const gfx::Animation* animation) {
  AnimationProgressed(animation);
}

void ConfirmQuitBubbleController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  // The browser process is about to exit.  Clean up |pref_change_registrar_|
  // now, otherwise it will outlive PrefService which will result in a crash
  // when it tries to remove itself as an observer of the PrefService in its
  // destructor.  Also explicitly set the state to quitting so we don't try to
  // show any more UI etc.
  pref_change_registrar_.reset();
  view_->Hide();
  state_ = State::kQuitting;
}

void ConfirmQuitBubbleController::OnTimerElapsed() {
  if (state_ == State::kPressed) {
    // The accelerator was held down the entire time the bubble was showing.
    confirm_quit::RecordHistogram(confirm_quit::kHoldDuration);
    ConfirmQuit();
  } else if (state_ == State::kReleased) {
    Reset();
  }
}

void ConfirmQuitBubbleController::OnConfirmToQuitPrefChanged() {
  if (browser_ && !browser_->profile()->GetPrefs()->GetBoolean(
                      prefs::kConfirmToQuitEnabled)) {
    Reset();
  }
}

void ConfirmQuitBubbleController::Reset() {
  DCHECK_NE(state_, State::kQuitting);
  if (state_ == State::kWaiting)
    return;
  state_ = State::kWaiting;
  second_press_start_time_ = base::TimeTicks();
  browser_ = nullptr;
  pref_change_registrar_.reset();
  view_->Hide();
  hide_timer_->Stop();
  browser_hide_animation_->Hide();
}

void ConfirmQuitBubbleController::ConfirmQuit() {
  DCHECK(state_ == State::kPressed || state_ == State::kReleased);
  state_ = State::kConfirmed;
  hide_timer_->Stop();
  browser_hide_animation_->Show();
}

void ConfirmQuitBubbleController::Quit() {
  DCHECK(state_ == State::kWaiting || state_ == State::kConfirmed);
  state_ = State::kQuitting;
  browser_ = nullptr;
  DoQuit();
}
