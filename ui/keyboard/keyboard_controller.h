// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/keyboard/container_behavior.h"
#include "ui/keyboard/container_type.h"
#include "ui/keyboard/display_util.h"
#include "ui/keyboard/keyboard_event_filter.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/keyboard/keyboard_layout_delegate.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/notification_manager.h"
#include "ui/keyboard/queued_container_type.h"
#include "ui/keyboard/queued_display_change.h"

namespace aura {
class Window;
}
namespace ui {
class InputMethod;
class TextInputClient;
}

namespace keyboard {

class CallbackAnimationObserver;
class KeyboardControllerObserver;
class KeyboardUI;

// Represents the current state of the keyboard managed by the controller.
// Don't change the numeric value of the members because they are used in UMA
// - VirtualKeyboard.ControllerStateTransition.
// - VirtualKeyboard.LingeringIntermediateState
enum class KeyboardControllerState {
  UNKNOWN = 0,
  // Keyboard has never been shown.
  INITIAL = 1,
  // Waiting for an extension to be loaded. Will move to HIDDEN if this is
  // loading pre-emptively, otherwise will move to SHOWN.
  LOADING_EXTENSION = 2,
  // Keyboard is shown.
  SHOWN = 4,
  // Keyboard is still shown, but will move to HIDING in a short period, or if
  // an input element gets focused again, will move to SHOWN.
  WILL_HIDE = 5,
  // Keyboard is hidden, but has shown at least once.
  HIDDEN = 7,
  COUNT,
};

// Provides control of the virtual keyboard, including providing a container
// and controlling visibility.
class KEYBOARD_EXPORT KeyboardController
    : public ui::InputMethodObserver,
      public aura::WindowObserver,
      public ui::InputMethodKeyboardController {
 public:

  KeyboardController();
  ~KeyboardController() override;

  // Enables the virtual keyboard with a specified |ui| and |delegate|.
  // Disables and re-enables the keyboard if it is already enabled.
  void EnableKeyboard(std::unique_ptr<KeyboardUI> ui,
                      KeyboardLayoutDelegate* delegate);

  // Disables the virtual keyboard. Resets the keyboard to its initial disabled
  // state and destroys the keyboard container window.
  // Does nothing if the keyboard is already disabled.
  void DisableKeyboard();

  // Attach the KeyboardUI contents window as a child of the given window.
  // Can only be called when the keyboard is not activated. |parent| must not
  // have any children.
  void ActivateKeyboardInContainer(aura::Window* parent);

  // Detach the KeyboardUI contents window from its parent container window.
  // Can only be called when the keyboard is activated. Explicitly hides the
  // keyboard if it is currently visible.
  void DeactivateKeyboard();

  // Returns the KeyboardUI contents window, or null if the keyboard contents
  // window has not been created yet.
  aura::Window* GetContentsWindow();

  // Returns the root window that this keyboard controller is attached to, or
  // null if the keyboard has not been attached to any root window.
  aura::Window* GetRootWindow();

  // Reloads the content of the keyboard. No-op if the keyboard content is not
  // loaded yet.
  void Reload();

  // Notifies the observer for contents bounds changed.
  void NotifyContentsBoundsChanging(const gfx::Rect& new_bounds);

  // Management of the observer list.
  void AddObserver(KeyboardControllerObserver* observer);
  bool HasObserver(KeyboardControllerObserver* observer) const;
  void RemoveObserver(KeyboardControllerObserver* observer);

  KeyboardUI* ui() { return ui_.get(); }

  void set_keyboard_locked(bool lock) { keyboard_locked_ = lock; }

  bool keyboard_locked() const { return keyboard_locked_; }

  // Hide the keyboard because the user has chosen to specifically hide the
  // keyboard, such as pressing the dismiss button.
  void HideKeyboardByUser();

  // Hide the keyboard due to some internally generated change to change the
  // state of the keyboard. For example, moving from the docked keyboard to the
  // floating keyboard.
  void HideKeyboardTemporarilyForTransition();

  // Hide the keyboard as an effect of a system action, such as opening the
  // settings page from the keyboard. There should be no reason the keyboard
  // should remain open.
  void HideKeyboardExplicitlyBySystem();

  // Hide the keyboard as a secondary effect of a system action, such as losing
  // focus of a text element. If focus is returned to any text element, it is
  // desirable to re-show the keyboard in this case.
  void HideKeyboardImplicitlyBySystem();

  // Force the keyboard to show up if not showing and lock the keyboard if
  // |lock| is true.
  void ShowKeyboard(bool lock);

  // Loads the keyboard UI contents in the background, but does not display
  // the keyboard.
  void LoadKeyboardUiInBackground();

  // Force the keyboard to show up in the specific display if not showing and
  // lock the keyboard
  void ShowKeyboardInDisplay(const display::Display& display);

  // Retrieves the active keyboard controller. Guaranteed to not be null while
  // there is an ash::Shell.
  static KeyboardController* Get();

  // Returns true if there is a valid KeyboardController instance (e.g. while
  // there is an ash::Shell).
  static bool HasInstance();

  // Returns true if keyboard is in SHOWN or SHOWING state.
  bool keyboard_visible() const;

  // Returns true if keyboard window has been created.
  bool IsKeyboardWindowCreated();

  // Returns the bounds in screen for the visible portion of the keyboard. An
  // empty rectangle will get returned when the keyboard is hidden.
  const gfx::Rect& visual_bounds_in_screen() const {
    return visual_bounds_in_screen_;
  }

  // Returns the current bounds that affect the workspace layout. If the
  // keyboard is not shown or if the keyboard mode should not affect the usable
  // region of the screen, an empty rectangle will be returned.
  gfx::Rect GetWorkspaceOccludedBounds() const;

  // Returns the current bounds that affect the window layout of the various
  // lock screens.
  gfx::Rect GetKeyboardLockScreenOffsetBounds() const;

  // Set the area on the screen that are occluded by the keyboard.
  void SetOccludedBounds(const gfx::Rect& bounds);

  KeyboardControllerState GetStateForTest() const { return state_; }

  ContainerType GetActiveContainerType() const {
    return container_behavior_->GetType();
  }

  gfx::Rect AdjustSetBoundsRequest(const gfx::Rect& display_bounds,
                                   const gfx::Rect& requested_bounds) const;

  // Returns true if overscroll is currently allowed by the active keyboard
  // container behavior.
  bool IsOverscrollAllowed() const;

  // Whether the keyboard is enabled.
  bool enabled() const { return ui_ != nullptr; }

  // Handle mouse and touch events on the keyboard. The effects of this method
  // will not stop propagation to the keyboard extension.
  bool HandlePointerEvent(const ui::LocatedEvent& event);

  // Moves an already loaded keyboard.
  void MoveKeyboard(const gfx::Rect& new_bounds);

  // Sets the active container type. If the keyboard is currently shown, this
  // will trigger a hide animation and a subsequent show animation. Otherwise
  // the ContainerBehavior change is synchronous.
  void SetContainerType(ContainerType type,
                        base::Optional<gfx::Rect> target_bounds,
                        base::OnceCallback<void(bool)> callback);

  // Sets floating keyboard draggable rect.
  bool SetDraggableArea(const gfx::Rect& rect);

  void MoveToDisplayWithTransition(display::Display display,
                                   gfx::Rect new_bounds_in_local);

  // Called by KeyboardUI when the keyboard contents have loaded. Shows
  // the keyboard if show_on_content_update_ is true.
  void NotifyContentsLoaded();

  // InputMethodKeyboardController overrides.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override;
  void RemoveObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  // For access to Observer methods for simulation.
  friend class KeyboardControllerTest;

  // For access to SetContainerBounds.
  friend class KeyboardLayoutManager;

  // For access to NotifyKeyboardConfigChanged
  friend bool keyboard::UpdateKeyboardConfig(
      const keyboard::KeyboardConfig& config);

  // Different ways to hide the keyboard.
  enum HideReason {
    // System initiated due to an active event, where the user does not want
    // to maintain any association with the previous text entry session.
    HIDE_REASON_SYSTEM_EXPLICIT,

    // System initiated due to a passive event, such as clicking on a non-text
    // control in a web page. Implicit hide events can be treated as passive
    // and can possibly be a transient loss of focus. This will generally cause
    // the keyboard to stay open for a brief moment and then hide, and possibly
    // come back if focus is regained within a short amount of time (transient
    // blur).
    HIDE_REASON_SYSTEM_IMPLICIT,

    // Keyboard is hidden temporarily for transitional reasons. Examples include
    // moving the keyboard to a different display (which closes it and re-opens
    // it on the new screen) or changing the container type (e.g. full-width to
    // floating)
    HIDE_REASON_SYSTEM_TEMPORARY,

    // User initiated.
    HIDE_REASON_USER_EXPLICIT,

  };

  // aura::WindowObserver overrides
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // InputMethodObserver overrides
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnFocus() override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnShowImeIfNeeded() override;

  // Sets the bounds of the container window.
  void SetContainerBounds(const gfx::Rect& new_bounds);

  // Show virtual keyboard immediately with animation.
  void ShowKeyboardInternal(const display::Display& display);
  void PopulateKeyboardContent(const display::Display& display,
                               bool show_keyboard);

  // Returns true if keyboard is scheduled to hide.
  bool WillHideKeyboard() const;

  // Immediately starts hiding animation of virtual keyboard and notifies
  // observers bounds change. This method forcibly sets keyboard_locked_
  // false while closing the keyboard.
  void HideKeyboard(HideReason reason);

  // Called when the hide animation finished.
  void HideAnimationFinished();
  // Called when the show animation finished.
  void ShowAnimationFinished();

  void NotifyKeyboardBoundsChangingAndEnsureCaretInWorkArea();

  // Called when the keyboard mode is set or the keyboard is moved to another
  // display.
  void AdjustKeyboardBounds();

  // Notifies keyboard config change to the observers.
  // Only called from |UpdateKeyboardConfig| in keyboard_util.
  void NotifyKeyboardConfigChanged();

  // Validates the state transition. Called from ChangeState.
  void CheckStateTransition(KeyboardControllerState prev,
                            KeyboardControllerState next);

  // Changes the current state and validates the transition.
  void ChangeState(KeyboardControllerState state);

  // Reports error histogram in case lingering in an intermediate state.
  void ReportLingeringState();

  // Shows the keyboard if the last time the keyboard was hidden was a small
  // time ago.
  void ShowKeyboardIfWithinTransientBlurThreshold();

  void SetContainerBehaviorInternal(ContainerType type);

  std::unique_ptr<KeyboardUI> ui_;
  KeyboardLayoutDelegate* layout_delegate_;

  // Container window that the keyboard UI contents window is a child of.
  aura::Window* parent_container_ = nullptr;

  // CallbackAnimationObserver should destructed before container_ because it
  // uses container_'s animator.
  std::unique_ptr<CallbackAnimationObserver> animation_observer_;

  // Current active visual behavior for the keyboard container.
  std::unique_ptr<ContainerBehavior> container_behavior_;

  std::unique_ptr<QueuedContainerType> queued_container_type_;
  std::unique_ptr<QueuedDisplayChange> queued_display_change_;

  // If true, show the keyboard window when keyboard UI content updates.
  bool show_on_content_update_;

  // If true, the keyboard is always visible even if no window has input focus.
  bool keyboard_locked_;
  KeyboardEventFilter event_filter_;

  base::ObserverList<KeyboardControllerObserver> observer_list_;

  // The bounds in screen for the visible portion of the keyboard.
  // If the contents window is visible, this should be the same size as the
  // contents window. If not, this should be empty.
  gfx::Rect visual_bounds_in_screen_;

  KeyboardControllerState state_;

  NotificationManager notification_manager_;

  base::Time time_of_last_blur_ = base::Time::UnixEpoch();

  DisplayUtil display_util_;

  base::WeakPtrFactory<KeyboardController> weak_factory_report_lingering_state_;
  base::WeakPtrFactory<KeyboardController> weak_factory_will_hide_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardController);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_H_
