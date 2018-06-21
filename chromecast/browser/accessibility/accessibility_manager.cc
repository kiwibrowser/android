// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/accessibility/accessibility_manager.h"

#include "chromecast/graphics/accessibility/focus_ring_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/public/activation_client.h"

namespace chromecast {
namespace shell {

AccessibilityManager::AccessibilityManager(
    aura::WindowTreeHost* window_tree_host)
    : window_tree_host_(window_tree_host) {
  DCHECK(window_tree_host);
  aura::Window* root_window = window_tree_host->window()->GetRootWindow();
  wm::ActivationClient* activation_client =
      wm::GetActivationClient(root_window);
  focus_ring_controller_ =
      std::make_unique<FocusRingController>(root_window, activation_client);
  accessibility_focus_ring_controller_ =
      std::make_unique<AccessibilityFocusRingController>(root_window);
  touch_exploration_manager_ = std::make_unique<TouchExplorationManager>(
      root_window, activation_client,
      accessibility_focus_ring_controller_.get());
}

AccessibilityManager::~AccessibilityManager() {}

void AccessibilityManager::SetFocusRingColor(SkColor color) {
  DCHECK(accessibility_focus_ring_controller_);
  accessibility_focus_ring_controller_->SetFocusRingColor(color);
}

void AccessibilityManager::ResetFocusRingColor() {
  DCHECK(accessibility_focus_ring_controller_);
  accessibility_focus_ring_controller_->ResetFocusRingColor();
}

void AccessibilityManager::SetFocusRing(
    const std::vector<gfx::Rect>& rects_in_screen,
    FocusRingBehavior focus_ring_behavior) {
  DCHECK(accessibility_focus_ring_controller_);
  accessibility_focus_ring_controller_->SetFocusRing(rects_in_screen,
                                                     focus_ring_behavior);
}

void AccessibilityManager::HideFocusRing() {
  DCHECK(accessibility_focus_ring_controller_);
  accessibility_focus_ring_controller_->HideFocusRing();
}

void AccessibilityManager::SetHighlights(
    const std::vector<gfx::Rect>& rects_in_screen,
    SkColor color) {
  DCHECK(accessibility_focus_ring_controller_);
  accessibility_focus_ring_controller_->SetHighlights(rects_in_screen, color);
}

void AccessibilityManager::HideHighlights() {
  DCHECK(accessibility_focus_ring_controller_);
  accessibility_focus_ring_controller_->HideHighlights();
}

void AccessibilityManager::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  touch_exploration_manager_->SetTouchAccessibilityAnchorPoint(anchor_point);
}

aura::WindowTreeHost* AccessibilityManager::window_tree_host() const {
  DCHECK(window_tree_host_);
  return window_tree_host_;
}

}  // namespace shell
}  // namespace chromecast
