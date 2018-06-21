// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/event.h"

namespace chromecast {
namespace shell {

// Describes visual context of the window within the UI.
enum class VisibilityType {
  // Unknown visibility state.
  UNKNOWN = 0,

  // Window is occupying the entire screen and can be interacted with.
  FULL_SCREEN = 1,

  // Window occupies a portion of the screen, supporting user interaction.
  PARTIAL_OUT = 2,

  // Window is hidden, and cannot be interacted with via touch.
  HIDDEN = 3,

  // Window is being displayed as a small visible tile.
  TILE = 4
};

// Represents requested activity windowing behavior. Behavior includes:
// 1. How long the activity should show
// 2. Whether the window should become immediately visible
// 3. How much screen space the window should occupy
// 4. What state to return to when the activity is completed
enum class VisibilityPriority {
  // Default priority. It is up to system to decide how to show the activity.
  DEFAULT = 0,

  // The activity wants to occupy the full screen for some period of time and
  // then become hidden after a timeout. When timeout, it returns to the
  // previous activity.
  TRANSIENT_TIMEOUTABLE = 1,

  // A high priority interruption occupies half of the screen if a sticky
  // activity is showing on the screen. Otherwise, it occupies the full screen.
  HIGH_PRIORITY_INTERRUPTION = 2,

  // The activity takes place of other activity and won't be timeout.
  STICKY_ACTIVITY = 3,

  // The activity stays on top of others (transient) but won't be timeout.
  // When the activity finishes, it returns to the previous one.
  TRANSIENT_STICKY = 4,

  // The activity should not be visible.
  HIDDEN = 5,
};

enum class GestureType { NO_GESTURE = 0, GO_BACK = 1, TAP = 2 };

// Class that represents the "window" a WebContents is displayed in cast_shell.
// For Linux, this represents an Aura window. For Android, this is a Activity.
// See CastContentWindowAura and CastContentWindowAndroid.
class CastContentWindow {
 public:
  class Delegate {
   public:
    // Notify window destruction.
    virtual void OnWindowDestroyed() {}

    // Notifies that a key event was triggered on the window.
    virtual void OnKeyEvent(const ui::KeyEvent& key_event) {}

    // Check to see if the gesture can be handled by the delegate. This is
    // called prior to ConsumeGesture().
    virtual bool CanHandleGesture(GestureType gesture_type) = 0;

    // Called while a system UI gesture is in progress.
    virtual void GestureProgress(GestureType gesture_type,
                                 const gfx::Point& touch_location){};

    // Called when an in-progress system UI gesture is cancelled (for example
    // when the finger is lifted before the completion of the gesture.)
    virtual void CancelGesture(GestureType gesture_type,
                               const gfx::Point& touch_location){};

    // Consume and handle a completed UI gesture. Returns whether the gesture
    // was handled or not.
    virtual bool ConsumeGesture(GestureType gesture_type) = 0;

    // Notify visibility change for this window.
    virtual void OnVisibilityChange(VisibilityType visibility_type) {}

    // Returns app ID of cast activity or application.
    virtual std::string GetId() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates the platform specific CastContentWindow. |delegate| should outlive
  // the created CastContentWindow.
  static std::unique_ptr<CastContentWindow> Create(
      CastContentWindow::Delegate* delegate,
      bool is_headless,
      bool enable_touch_input);

  virtual ~CastContentWindow() {}

  // Creates a full-screen window for |web_contents| and displays it if
  // |is_visible| is true.
  // |web_contents| should outlive this CastContentWindow.
  // |window_manager| should outlive this CastContentWindow.
  // TODO(seantopping): This method probably shouldn't exist; this class should
  // use RAII instead.
  virtual void CreateWindowForWebContents(
      content::WebContents* web_contents,
      CastWindowManager* window_manager,
      bool is_visible,
      CastWindowManager::WindowId z_order,
      VisibilityPriority visibility_priority) = 0;

  // Enables touch input to be routed to the window's WebContents.
  virtual void EnableTouchInput(bool enabled) = 0;

  // Cast activity or application calls it to request for a visibility priority
  // change.
  virtual void RequestVisibility(VisibilityPriority visibility_priority) = 0;

  // Notify the window that its visibility type has changed. This should only
  // ever be called by the window manager.
  // TODO(seantopping): Make this private to the window manager.
  virtual void NotifyVisibilityChange(VisibilityType visibility_type) = 0;

  // Cast activity or application calls it to request for moving out of the
  // screen.
  virtual void RequestMoveOut() = 0;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
