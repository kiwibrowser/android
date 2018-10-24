// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/path.h"
#include "ui/keyboard/container_floating_behavior.h"
#include "ui/keyboard/container_full_width_behavior.h"
#include "ui/keyboard/container_fullscreen_behavior.h"
#include "ui/keyboard/container_type.h"
#include "ui/keyboard/display_util.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_layout_manager.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/notification_manager.h"
#include "ui/keyboard/queued_container_type.h"
#include "ui/keyboard/queued_display_change.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/wm/core/window_animations.h"

namespace {

// Owned by ash::Shell.
keyboard::KeyboardController* g_keyboard_controller = nullptr;

constexpr int kHideKeyboardDelayMs = 100;

// Reports an error histogram if the keyboard state is lingering in an
// intermediate state for more than 5 seconds.
constexpr int kReportLingeringStateDelayMs = 5000;

// Delay threshold after the keyboard enters the WILL_HIDE state. If text focus
// is regained during this threshold, the keyboard will show again, even if it
// is an asynchronous event. This is for the benefit of things like login flow
// where the password field may get text focus after an animation that plays
// after the user enters their username.
constexpr int kTransientBlurThresholdMs = 3500;

// State transition diagram (document linked from crbug.com/719905)
bool isAllowedStateTransition(keyboard::KeyboardControllerState from,
                              keyboard::KeyboardControllerState to) {
  static const std::set<std::pair<keyboard::KeyboardControllerState,
                                  keyboard::KeyboardControllerState>>
      kAllowedStateTransition = {
          // The initial ShowKeyboard scenario
          // INITIAL -> LOADING_EXTENSION -> HIDDEN -> SHOWN.
          {keyboard::KeyboardControllerState::UNKNOWN,
           keyboard::KeyboardControllerState::INITIAL},
          {keyboard::KeyboardControllerState::INITIAL,
           keyboard::KeyboardControllerState::LOADING_EXTENSION},
          {keyboard::KeyboardControllerState::LOADING_EXTENSION,
           keyboard::KeyboardControllerState::HIDDEN},
          {keyboard::KeyboardControllerState::HIDDEN,
           keyboard::KeyboardControllerState::SHOWN},

          // Hide scenario
          // SHOWN -> WILL_HIDE -> HIDDEN.
          {keyboard::KeyboardControllerState::SHOWN,
           keyboard::KeyboardControllerState::WILL_HIDE},
          {keyboard::KeyboardControllerState::WILL_HIDE,
           keyboard::KeyboardControllerState::HIDDEN},

          // Focus transition scenario
          // SHOWN -> WILL_HIDE -> SHOWN.
          {keyboard::KeyboardControllerState::WILL_HIDE,
           keyboard::KeyboardControllerState::SHOWN},

          // HideKeyboard can be called at anytime for example on shutdown.
          {keyboard::KeyboardControllerState::SHOWN,
           keyboard::KeyboardControllerState::HIDDEN},
      };
  return kAllowedStateTransition.count(std::make_pair(from, to)) == 1;
};

void SetTouchEventLogging(bool enable) {
  // TODO(moshayedi): crbug.com/642863. Revisit when we have mojo interface for
  // InputController for processes that aren't mus-ws.
  if (aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS)
    return;
  ui::InputController* controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  if (controller)
    controller->SetTouchEventLoggingEnabled(enable);
}

std::string StateToStr(keyboard::KeyboardControllerState state) {
  switch (state) {
    case keyboard::KeyboardControllerState::UNKNOWN:
      return "UNKNOWN";
    case keyboard::KeyboardControllerState::SHOWN:
      return "SHOWN";
    case keyboard::KeyboardControllerState::LOADING_EXTENSION:
      return "LOADING_EXTENSION";
    case keyboard::KeyboardControllerState::WILL_HIDE:
      return "WILL_HIDE";
    case keyboard::KeyboardControllerState::HIDDEN:
      return "HIDDEN";
    case keyboard::KeyboardControllerState::INITIAL:
      return "INITIAL";
    case keyboard::KeyboardControllerState::COUNT:
      NOTREACHED();
  }
  NOTREACHED() << "Unknownstate: " << static_cast<int>(state);
  // Needed for windows build.
  return "";
}

}  // namespace

namespace keyboard {

// Observer for both keyboard show and hide animations. It should be owned by
// KeyboardController.
class CallbackAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  CallbackAnimationObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

 private:
  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (WasAnimationAbortedForProperty(ui::LayerAnimationElement::TRANSFORM) ||
        WasAnimationAbortedForProperty(ui::LayerAnimationElement::OPACITY)) {
      return;
    }
    DCHECK(
        WasAnimationCompletedForProperty(ui::LayerAnimationElement::TRANSFORM));
    DCHECK(
        WasAnimationCompletedForProperty(ui::LayerAnimationElement::OPACITY));
    std::move(callback_).Run();
  }

  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackAnimationObserver);
};

KeyboardController::KeyboardController()
    : weak_factory_report_lingering_state_(this),
      weak_factory_will_hide_(this) {
  DCHECK_EQ(g_keyboard_controller, nullptr);
  g_keyboard_controller = this;
}

KeyboardController::~KeyboardController() {
  DCHECK(g_keyboard_controller);
  DCHECK(!enabled())
      << "Keyboard must be disabled before KeyboardController is destroyed";
  g_keyboard_controller = nullptr;
}

void KeyboardController::EnableKeyboard(std::unique_ptr<KeyboardUI> ui,
                                        KeyboardLayoutDelegate* delegate) {
  if (enabled())
    DisableKeyboard();

  ui_ = std::move(ui);

  layout_delegate_ = delegate;
  show_on_content_update_ = false;
  keyboard_locked_ = false;
  state_ = KeyboardControllerState::UNKNOWN;
  ui_->GetInputMethod()->AddObserver(this);
  ui_->SetController(this);
  SetContainerBehaviorInternal(ContainerType::FULL_WIDTH);
  ChangeState(KeyboardControllerState::INITIAL);
  visual_bounds_in_screen_ = gfx::Rect();
  time_of_last_blur_ = base::Time::UnixEpoch();
}

void KeyboardController::DisableKeyboard() {
  if (!enabled())
    return;

  if (parent_container_)
    DeactivateKeyboard();

  // TODO(https://crbug.com/731537): Move KeyboardController members into a
  // subobject so we can just put this code into the subobject destructor.
  queued_display_change_.reset();
  queued_container_type_.reset();
  container_behavior_.reset();
  animation_observer_.reset();

  ui_->GetInputMethod()->RemoveObserver(this);
  for (KeyboardControllerObserver& observer : observer_list_)
    observer.OnKeyboardClosed();
  ui_->SetController(nullptr);
  ui_.reset();
}

void KeyboardController::ActivateKeyboardInContainer(aura::Window* parent) {
  DCHECK(parent);
  DCHECK(!parent_container_);
  parent_container_ = parent;
  // Observe changes to root window bounds.
  parent_container_->GetRootWindow()->AddObserver(this);

  if (GetContentsWindow()) {
    DCHECK(!GetContentsWindow()->parent());
    parent_container_->AddChild(GetContentsWindow());
  }
}

void KeyboardController::DeactivateKeyboard() {
  DCHECK(parent_container_);

  // Ensure the keyboard is not visible before deactivating it.
  HideKeyboardExplicitlyBySystem();

  if (GetContentsWindow() && GetContentsWindow()->parent()) {
    DCHECK_EQ(parent_container_, GetContentsWindow()->parent());
    parent_container_->RemoveChild(GetContentsWindow());
  }
  parent_container_->GetRootWindow()->RemoveObserver(this);
  parent_container_ = nullptr;
}

// static
KeyboardController* KeyboardController::Get() {
  DCHECK(g_keyboard_controller);
  return g_keyboard_controller;
}

// static
bool KeyboardController::HasInstance() {
  return g_keyboard_controller;
}

bool KeyboardController::keyboard_visible() const {
  return state_ == KeyboardControllerState::SHOWN;
}

aura::Window* KeyboardController::GetContentsWindow() {
  return ui_ && ui_->HasContentsWindow() ? ui_->GetContentsWindow() : nullptr;
}

aura::Window* KeyboardController::GetRootWindow() {
  return parent_container_ ? parent_container_->GetRootWindow() : nullptr;
}

void KeyboardController::NotifyContentsBoundsChanging(
    const gfx::Rect& new_bounds) {
  visual_bounds_in_screen_ = new_bounds;
  if (ui_->HasContentsWindow() && ui_->GetContentsWindow()->IsVisible()) {
    const gfx::Rect occluded_bounds =
        container_behavior_->GetOccludedBounds(new_bounds);
    notification_manager_.SendNotifications(
        container_behavior_->OccludedBoundsAffectWorkspaceLayout(),
        keyboard_locked(), new_bounds, occluded_bounds, observer_list_);

    if (keyboard::IsKeyboardOverscrollEnabled())
      ui_->InitInsets(occluded_bounds);
    else
      ui_->ResetInsets();
  } else {
    visual_bounds_in_screen_ = gfx::Rect();
  }
}

void KeyboardController::MoveKeyboard(const gfx::Rect& new_bounds) {
  DCHECK(keyboard_visible());
  SetContainerBounds(new_bounds);
}

void KeyboardController::SetContainerBounds(const gfx::Rect& new_bounds) {
  ui::LayerAnimator* animator = GetContentsWindow()->layer()->GetAnimator();
  // Stops previous animation if a window resize is requested during animation.
  if (animator->is_animating())
    animator->StopAnimating();

  GetContentsWindow()->SetBounds(new_bounds);

  // We need to send out this notification only if keyboard is visible since
  // the contents window is resized even if keyboard is hidden.
  if (keyboard_visible())
    NotifyContentsBoundsChanging(new_bounds);
}

void KeyboardController::NotifyContentsLoaded() {
  const bool should_show = show_on_content_update_;
  if (state_ == KeyboardControllerState::LOADING_EXTENSION)
    ChangeState(KeyboardControllerState::HIDDEN);
  if (should_show) {
    // The window height is set to 0 initially or before switch to an IME in a
    // different extension. Virtual keyboard window may wait for this bounds
    // change to correctly animate in.
    if (keyboard_locked()) {
      // Do not move the keyboard to another display after switch to an IME in
      // a different extension.
      ShowKeyboardInDisplay(
          display_util_.GetNearestDisplayToWindow(GetContentsWindow()));
    } else {
      ShowKeyboard(false /* lock */);
    }
  }
}

void KeyboardController::AddObserver(KeyboardControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

bool KeyboardController::HasObserver(
    KeyboardControllerObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void KeyboardController::RemoveObserver(KeyboardControllerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void KeyboardController::MoveToDisplayWithTransition(
    display::Display display,
    gfx::Rect new_bounds_in_local) {
  queued_display_change_ =
      std::make_unique<QueuedDisplayChange>(display, new_bounds_in_local);
  HideKeyboardTemporarilyForTransition();
}

void KeyboardController::HideKeyboard(HideReason reason) {
  TRACE_EVENT0("vk", "HideKeyboard");

  switch (state_) {
    case KeyboardControllerState::INITIAL:
    case KeyboardControllerState::HIDDEN:
      return;
    case KeyboardControllerState::LOADING_EXTENSION:
      show_on_content_update_ = false;
      return;

    case KeyboardControllerState::WILL_HIDE:
    case KeyboardControllerState::SHOWN: {
      SetTouchEventLogging(true /* enable */);

      // Log whether this was a user or system (automatic) action.
      switch (reason) {
        case HIDE_REASON_SYSTEM_EXPLICIT:
        case HIDE_REASON_SYSTEM_IMPLICIT:
        case HIDE_REASON_SYSTEM_TEMPORARY:
          keyboard::LogKeyboardControlEvent(
              keyboard::KEYBOARD_CONTROL_HIDE_AUTO);
          break;
        case HIDE_REASON_USER_EXPLICIT:
          keyboard::LogKeyboardControlEvent(
              keyboard::KEYBOARD_CONTROL_HIDE_USER);
          break;
      }

      // Decide whether regaining focus in a web-based text field should cause
      // the keyboard to come back.
      switch (reason) {
        case HIDE_REASON_SYSTEM_IMPLICIT:
          time_of_last_blur_ = base::Time::Now();
          break;

        case HIDE_REASON_SYSTEM_TEMPORARY:
        case HIDE_REASON_SYSTEM_EXPLICIT:
        case HIDE_REASON_USER_EXPLICIT:
          time_of_last_blur_ = base::Time::UnixEpoch();
          break;
      }

      NotifyContentsBoundsChanging(gfx::Rect());

      set_keyboard_locked(false);

      aura::Window* window = GetContentsWindow();
      DCHECK(window);

      animation_observer_ = std::make_unique<CallbackAnimationObserver>(
          base::BindOnce(&KeyboardController::HideAnimationFinished,
                         base::Unretained(this)));
      ui::ScopedLayerAnimationSettings layer_animation_settings(
          window->layer()->GetAnimator());
      layer_animation_settings.AddObserver(animation_observer_.get());

      {
        // Scoped settings go into effect when scope ends.
        ::wm::ScopedHidingAnimationSettings hiding_settings(window);
        container_behavior_->DoHidingAnimation(window, &hiding_settings);
      }

      ui_->HideKeyboardContainer(window);
      ChangeState(KeyboardControllerState::HIDDEN);

      for (KeyboardControllerObserver& observer : observer_list_)
        observer.OnKeyboardHidden();
      ui_->EnsureCaretInWorkArea(gfx::Rect());

      break;
    }
    default:
      NOTREACHED();
  }
}

void KeyboardController::HideKeyboardByUser() {
  HideKeyboard(HIDE_REASON_USER_EXPLICIT);
}

void KeyboardController::HideKeyboardTemporarilyForTransition() {
  HideKeyboard(HIDE_REASON_SYSTEM_TEMPORARY);
}

void KeyboardController::HideKeyboardExplicitlyBySystem() {
  HideKeyboard(HIDE_REASON_SYSTEM_EXPLICIT);
}

void KeyboardController::HideKeyboardImplicitlyBySystem() {
  if (state_ != KeyboardControllerState::SHOWN || keyboard_locked())
    return;

  ChangeState(KeyboardControllerState::WILL_HIDE);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&KeyboardController::HideKeyboard,
                     weak_factory_will_hide_.GetWeakPtr(),
                     HIDE_REASON_SYSTEM_IMPLICIT),
      base::TimeDelta::FromMilliseconds(kHideKeyboardDelayMs));
}

void KeyboardController::DismissVirtualKeyboard() {
  HideKeyboardByUser();
}

void KeyboardController::HideAnimationFinished() {
  if (state_ == KeyboardControllerState::HIDDEN) {
    if (queued_container_type_) {
      SetContainerBehaviorInternal(queued_container_type_->container_type());
      // The position of the container window will be adjusted shortly in
      // |PopulateKeyboardContent| before showing animation, so we can set the
      // passed bounds directly.
      if (queued_container_type_->target_bounds())
        SetContainerBounds(queued_container_type_->target_bounds().value());
      ShowKeyboard(false /* lock */);
    }

    if (queued_display_change_) {
      ShowKeyboardInDisplay(queued_display_change_->new_display());
      SetContainerBounds(queued_display_change_->new_bounds_in_local());
      queued_display_change_ = nullptr;
    }
  }
}

void KeyboardController::ShowAnimationFinished() {
  MarkKeyboardLoadFinished();
  NotifyKeyboardBoundsChangingAndEnsureCaretInWorkArea();
}

void KeyboardController::SetContainerBehaviorInternal(
    const ContainerType type) {
  switch (type) {
    case ContainerType::FULL_WIDTH:
      container_behavior_ = std::make_unique<ContainerFullWidthBehavior>(this);
      break;
    case ContainerType::FLOATING:
      container_behavior_ = std::make_unique<ContainerFloatingBehavior>(this);
      break;
    case ContainerType::FULLSCREEN:
      container_behavior_ = std::make_unique<ContainerFullscreenBehavior>(this);
      break;
    default:
      NOTREACHED();
  }
}

void KeyboardController::ShowKeyboard(bool lock) {
  set_keyboard_locked(lock);
  ShowKeyboardInternal(display::Display());
}

void KeyboardController::ShowKeyboardInDisplay(
    const display::Display& display) {
  set_keyboard_locked(true);
  ShowKeyboardInternal(display);
}

bool KeyboardController::IsKeyboardWindowCreated() {
  return ui_->HasContentsWindow();
}

void KeyboardController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.new_parent && params.target == GetContentsWindow())
    OnTextInputStateChanged(ui_->GetInputMethod()->GetTextInputClient());
}

void KeyboardController::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!window->GetRootWindow()->HasObserver(this))
    window->GetRootWindow()->AddObserver(this);
  AdjustKeyboardBounds();
}

void KeyboardController::OnWindowRemovingFromRootWindow(aura::Window* window,
    aura::Window* new_root) {
  if (window->GetRootWindow()->HasObserver(this))
    window->GetRootWindow()->RemoveObserver(this);
}

void KeyboardController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!window->IsRootWindow())
    return;
  // Keep the same height when window resizes. It gets called when the screen
  // rotates.
  if (!ui_->HasContentsWindow())
    return;

  container_behavior_->SetCanonicalBounds(GetContentsWindow(), new_bounds);
}

void KeyboardController::Reload() {
  if (ui_->HasContentsWindow()) {
    // A reload should never try to show virtual keyboard. If keyboard is not
    // visible before reload, it should stay invisible after reload.
    show_on_content_update_ = false;
    ui_->ReloadKeyboardIfNeeded();
  }
}

void KeyboardController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  TRACE_EVENT0("vk", "OnTextInputStateChanged");

  bool focused =
      client && (client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE &&
                 client->GetTextInputMode() != ui::TEXT_INPUT_MODE_NONE);
  bool should_hide = !focused && container_behavior_->TextBlurHidesKeyboard();
  bool is_web =
      client && client->GetTextInputFlags() != ui::TEXT_INPUT_FLAG_NONE;

  if (should_hide) {
    switch (state_) {
      case KeyboardControllerState::LOADING_EXTENSION:
        show_on_content_update_ = false;
        return;
      case KeyboardControllerState::SHOWN:
        HideKeyboardImplicitlyBySystem();
        return;
      default:
        return;
    }
  } else {
    switch (state_) {
      case KeyboardControllerState::WILL_HIDE:
        // Abort a pending keyboard hide.
        ChangeState(KeyboardControllerState::SHOWN);
        return;
      case KeyboardControllerState::HIDDEN:
        if (focused && is_web)
          ShowKeyboardIfWithinTransientBlurThreshold();
        return;
      default:
        break;
    }
    // Do not explicitly show the Virtual keyboard unless it is in the process
    // of hiding or the hide duration was very short (transient blur). Instead,
    // the virtual keyboard is shown in response to a user gesture (mouse or
    // touch) that is received while an element has input focus. Showing the
    // keyboard requires an explicit call to OnShowImeIfNeeded.
  }
}

void KeyboardController::ShowKeyboardIfWithinTransientBlurThreshold() {
  static const base::TimeDelta kTransientBlurThreshold =
      base::TimeDelta::FromMilliseconds(kTransientBlurThresholdMs);

  const base::Time now = base::Time::Now();
  const base::TimeDelta time_since_last_blur = now - time_of_last_blur_;
  if (time_since_last_blur < kTransientBlurThreshold)
    ShowKeyboard(false);
}

void KeyboardController::OnShowImeIfNeeded() {
  // Calling |ShowKeyboardInternal| may move the keyboard to another display.
  if (IsKeyboardEnabled() && !keyboard_locked())
    ShowKeyboardInternal(display::Display());
}

void KeyboardController::LoadKeyboardUiInBackground() {
  // ShowKeyboardInternal may trigger RootControllerWindow::ActiveKeyboard which
  // will cause LoadKeyboardUiInBackground to potentially run even though the
  // keyboard has been initialized.
  if (state_ != KeyboardControllerState::INITIAL)
    return;

  PopulateKeyboardContent(display::Display(), false);
}

void KeyboardController::ShowKeyboardInternal(const display::Display& display) {
  keyboard::MarkKeyboardLoadStarted();
  PopulateKeyboardContent(display, true);
}

void KeyboardController::PopulateKeyboardContent(
    const display::Display& display,
    bool show_keyboard) {
  DCHECK(show_keyboard || state_ == KeyboardControllerState::INITIAL);

  TRACE_EVENT0("vk", "PopulateKeyboardContent");

  if (parent_container_->children().empty()) {
    DCHECK_EQ(state_, KeyboardControllerState::INITIAL);
    aura::Window* contents = ui_->GetContentsWindow();
    parent_container_->AddChild(contents);
  }

  DCHECK(ui_->HasContentsWindow());
  if (layout_delegate_ != nullptr) {
    if (display.is_valid())
      layout_delegate_->MoveKeyboardToDisplay(display);
    else
      layout_delegate_->MoveKeyboardToTouchableDisplay();
  }

  aura::Window* contents = ui_->GetContentsWindow();
  DCHECK_EQ(parent_container_, contents->parent());

  switch (state_) {
    case KeyboardControllerState::SHOWN:
      return;
    case KeyboardControllerState::LOADING_EXTENSION:
      show_on_content_update_ |= show_keyboard;
      return;
    default:
      break;
  }

  ui_->ReloadKeyboardIfNeeded();

  SetTouchEventLogging(!show_keyboard /* enable */);

  switch (state_) {
    case KeyboardControllerState::INITIAL:
      DCHECK_EQ(ui_->GetContentsWindow()->bounds().height(), 0);
      show_on_content_update_ = show_keyboard;
      ChangeState(KeyboardControllerState::LOADING_EXTENSION);
      return;
    case KeyboardControllerState::WILL_HIDE:
      ChangeState(KeyboardControllerState::SHOWN);
      return;
    case KeyboardControllerState::HIDDEN: {
      // If the container is not animating, makes sure the position and opacity
      // are at begin states for animation.
      container_behavior_->InitializeShowAnimationStartingState(contents);
      break;
    }
    default:
      NOTREACHED();
  }

  DCHECK_EQ(state_, KeyboardControllerState::HIDDEN);

  keyboard::LogKeyboardControlEvent(keyboard::KEYBOARD_CONTROL_SHOW);

  ui::LayerAnimator* container_animator = contents->layer()->GetAnimator();
  container_animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  ui_->ShowKeyboardContainer(contents);

  animation_observer_ =
      std::make_unique<CallbackAnimationObserver>(base::BindOnce(
          &KeyboardController::ShowAnimationFinished, base::Unretained(this)));
  ui::ScopedLayerAnimationSettings settings(container_animator);
  settings.AddObserver(animation_observer_.get());

  container_behavior_->DoShowingAnimation(contents, &settings);

  // the queued container behavior will notify JS to change layout when it
  // gets destroyed.
  queued_container_type_ = nullptr;

  ChangeState(KeyboardControllerState::SHOWN);
}

bool KeyboardController::WillHideKeyboard() const {
  bool res = weak_factory_will_hide_.HasWeakPtrs();
  DCHECK_EQ(res, state_ == KeyboardControllerState::WILL_HIDE);
  return res;
}

void KeyboardController::
    NotifyKeyboardBoundsChangingAndEnsureCaretInWorkArea() {
  // Notify observers after animation finished to prevent reveal desktop
  // background during animation.
  NotifyContentsBoundsChanging(GetContentsWindow()->bounds());
  ui_->EnsureCaretInWorkArea(
      container_behavior_->GetOccludedBounds(GetContentsWindow()->bounds()));
}

void KeyboardController::NotifyKeyboardConfigChanged() {
  for (KeyboardControllerObserver& observer : observer_list_)
    observer.OnKeyboardConfigChanged();
}

void KeyboardController::AdjustKeyboardBounds() {
  container_behavior_->SetCanonicalBounds(GetContentsWindow(),
                                          GetRootWindow()->bounds());
}

void KeyboardController::CheckStateTransition(KeyboardControllerState prev,
                                              KeyboardControllerState next) {
  std::stringstream error_message;
  const bool valid_transition = isAllowedStateTransition(prev, next);
  if (!valid_transition)
    error_message << "Unexpected transition";

  // Emit UMA
  const int transition_record =
      (valid_transition ? 1 : -1) *
      (static_cast<int>(prev) * 1000 + static_cast<int>(next));
  base::UmaHistogramSparse("VirtualKeyboard.ControllerStateTransition",
                           transition_record);
  UMA_HISTOGRAM_BOOLEAN("VirtualKeyboard.ControllerStateTransitionIsValid",
                        transition_record > 0);

  DCHECK(error_message.str().empty())
      << "State: " << StateToStr(prev) << " -> " << StateToStr(next) << " "
      << error_message.str();
}

void KeyboardController::ChangeState(KeyboardControllerState state) {
  CheckStateTransition(state_, state);
  if (state_ == state)
    return;

  state_ = state;

  if (state != KeyboardControllerState::WILL_HIDE)
    weak_factory_will_hide_.InvalidateWeakPtrs();
  if (state != KeyboardControllerState::LOADING_EXTENSION)
    show_on_content_update_ = false;
  for (KeyboardControllerObserver& observer : observer_list_)
    observer.OnStateChanged(state);

  weak_factory_report_lingering_state_.InvalidateWeakPtrs();
  switch (state_) {
    case KeyboardControllerState::LOADING_EXTENSION:
    case KeyboardControllerState::WILL_HIDE:
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&KeyboardController::ReportLingeringState,
                         weak_factory_report_lingering_state_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kReportLingeringStateDelayMs));
      break;
    default:
      // Do nothing
      break;
  }
}

void KeyboardController::ReportLingeringState() {
  UMA_HISTOGRAM_ENUMERATION("VirtualKeyboard.LingeringIntermediateState",
                            state_, KeyboardControllerState::COUNT);
}

gfx::Rect KeyboardController::GetWorkspaceOccludedBounds() const {
  return container_behavior_->GetOccludedBounds(visual_bounds_in_screen_);
}

gfx::Rect KeyboardController::GetKeyboardLockScreenOffsetBounds() const {
  // Overscroll is generally dependent on lock state, however, its behavior
  // temporarily overridden by a static field in certain lock screen contexts.
  // Furthermore, floating keyboard should never affect layout.
  if (keyboard_visible() && !keyboard::IsKeyboardOverscrollEnabled() &&
      container_behavior_->GetType() != ContainerType::FLOATING &&
      container_behavior_->GetType() != ContainerType::FULLSCREEN) {
    return visual_bounds_in_screen_;
  }
  return gfx::Rect();
}

void KeyboardController::SetOccludedBounds(const gfx::Rect& bounds) {
  if (container_behavior_->GetType() != ContainerType::FULLSCREEN)
    return;

  static_cast<ContainerFullscreenBehavior&>(*container_behavior_)
      .SetOccludedBounds(bounds);

  // Notify that only the occluded bounds have changed.
  if (keyboard_visible())
    NotifyContentsBoundsChanging(visual_bounds_in_screen_);
}

gfx::Rect KeyboardController::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds) const {
  return container_behavior_->AdjustSetBoundsRequest(display_bounds,
                                                     requested_bounds);
}

bool KeyboardController::IsOverscrollAllowed() const {
  return container_behavior_->IsOverscrollAllowed();
}

bool KeyboardController::HandlePointerEvent(const ui::LocatedEvent& event) {
  const display::Display& current_display =
      display_util_.GetNearestDisplayToWindow(GetRootWindow());
  return container_behavior_->HandlePointerEvent(event, current_display);
}

void KeyboardController::SetContainerType(
    const ContainerType type,
    base::Optional<gfx::Rect> target_bounds,
    base::OnceCallback<void(bool)> callback) {
  if (container_behavior_->GetType() == type) {
    std::move(callback).Run(false);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("InputMethod.VirtualKeyboard.ContainerBehavior",
                            type, ContainerType::COUNT);

  if (state_ == KeyboardControllerState::SHOWN) {
    // Keyboard is already shown. Hiding the keyboard at first then switching
    // container type.
    queued_container_type_ = std::make_unique<QueuedContainerType>(
        this, type, target_bounds, std::move(callback));
    HideKeyboard(HIDE_REASON_SYSTEM_TEMPORARY);
  } else {
    // Keyboard is hidden. Switching the container type immediately and invoking
    // the passed callback now.
    SetContainerBehaviorInternal(type);
    if (target_bounds)
      SetContainerBounds(target_bounds.value());
    DCHECK_EQ(GetActiveContainerType(), type);
    std::move(callback).Run(true /* change_successful */);
  }
}

bool KeyboardController::SetDraggableArea(const gfx::Rect& rect) {
  return container_behavior_->SetDraggableArea(rect);
}

bool KeyboardController::DisplayVirtualKeyboard() {
  // Calling |ShowKeyboardInternal| may move the keyboard to another display.
  if (IsKeyboardEnabled() && !keyboard_locked()) {
    ShowKeyboardInternal(display::Display());
    return true;
  }
  return false;
}
void KeyboardController::AddObserver(
    ui::InputMethodKeyboardControllerObserver* observer) {
  // TODO: Implement me
}

void KeyboardController::RemoveObserver(
    ui::InputMethodKeyboardControllerObserver* observer) {
  // TODO: Implement me
}

bool KeyboardController::IsKeyboardVisible() {
  return keyboard_visible();
}

}  // namespace keyboard
