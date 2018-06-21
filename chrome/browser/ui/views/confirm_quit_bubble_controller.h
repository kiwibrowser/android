// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_QUIT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_QUIT_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/animation_delegate.h"

class ConfirmQuitBubbleBase;
class PrefChangeRegistrar;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace gfx {
class SlideAnimation;
}

// Manages showing and hiding the confirm-to-quit bubble.  Requests Chrome to be
// closed if the quit accelerator is held down or pressed twice in succession.
// The singleton instance of this class is added as a PreTargetHandler for each
// browser window.
class ConfirmQuitBubbleController : public gfx::AnimationDelegate,
                                    public BrowserListObserver,
                                    public content::NotificationObserver,
                                    public ui::EventHandler {
 public:
  static ConfirmQuitBubbleController* GetInstance();

  ~ConfirmQuitBubbleController() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 protected:
  // |animation| is used to fade out all browser windows.
  ConfirmQuitBubbleController(std::unique_ptr<ConfirmQuitBubbleBase> bubble,
                              std::unique_ptr<base::Timer> hide_timer,
                              std::unique_ptr<gfx::SlideAnimation> animation);

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // Runs the quit action now.  Virtual so tests can override the quit action.
  virtual void DoQuit();

  // Returns true if the confirm-to-quit feature is enabled.  Virtual so tests
  // can override the setting.
  virtual bool IsFeatureEnabled();

 private:
  friend struct base::DefaultSingletonTraits<ConfirmQuitBubbleController>;

  enum class State {
    // The accelerator has not been pressed.
    kWaiting,

    // The accelerator was pressed, but not yet released.
    kPressed,

    // The accelerator was pressed and released before the timer expired.
    kReleased,

    // The accelerator was either held down for the entire duration of the
    // timer, or was pressed a second time.  Either way, the accelerator is
    // currently held.
    kConfirmed,

    // The accelerator was released and Chrome is now quitting.
    kQuitting,
  };

  ConfirmQuitBubbleController();

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void OnTimerElapsed();

  // Called when the user changes their preference for the confirm-to-quit
  // setting.
  void OnConfirmToQuitPrefChanged();

  // Resets back to the waiting state.  Hides any UI and resets timers that may
  // be active.
  void Reset();

  // Transitions to the confirmed state.  Quit() will be run later when the user
  // releases the accelerator.
  void ConfirmQuit();

  // Updates state and calls DoQuit().
  void Quit();

  std::unique_ptr<ConfirmQuitBubbleBase> const view_;

  State state_;

  // Used only to distinguish between a double-press and a tap-and-hold when
  // recording metrics.
  base::TimeTicks second_press_start_time_;

  // The last active browser when the accelerator was pressed.
  Browser* browser_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  std::unique_ptr<base::Timer> hide_timer_;

  std::unique_ptr<gfx::SlideAnimation> const browser_hide_animation_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmQuitBubbleController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_QUIT_BUBBLE_CONTROLLER_H_
