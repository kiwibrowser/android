// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_mash.h"

#include <stdint.h>

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_properties.mojom.h"
#include "ash/public/interfaces/window_style.mojom.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/window_manager_frame_values.h"

BrowserFrameMash::BrowserFrameMash(BrowserFrame* browser_frame,
                                   BrowserView* browser_view)
    : views::DesktopNativeWidgetAura(browser_frame),
      browser_frame_(browser_frame),
      browser_view_(browser_view) {
  DCHECK(browser_frame_);
  DCHECK(browser_view_);
  DCHECK(!features::IsAshInBrowserProcess());
}

BrowserFrameMash::~BrowserFrameMash() {}

views::Widget::InitParams BrowserFrameMash::GetWidgetParams() {
  views::Widget::InitParams params;
  params.name = "BrowserFrame";
  params.native_widget = this;
  chrome::GetSavedWindowBoundsAndShowState(browser_view_->browser(),
                                           &params.bounds, &params.show_state);
  params.delegate = browser_view_;
  std::map<std::string, std::vector<uint8_t>> properties =
      views::MusClient::ConfigurePropertiesFromParams(params);
  // Indicates mash shouldn't handle immersive, rather we will.
  properties[ui::mojom::WindowManager::kDisableImmersive_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(true);

  Browser* browser = browser_view_->browser();
  properties[ash::mojom::kAshWindowStyle_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ash::mojom::WindowStyle::BROWSER));
  // ChromeLauncherController manages the browser shortcut shelf item; set the
  // window's shelf item type property to be ignored by ash::ShelfWindowWatcher.
  properties[ui::mojom::WindowManager::kShelfItemType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(ash::TYPE_BROWSER_SHORTCUT));
  properties[ui::mojom::WindowManager::kWindowTitleShown_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(browser_view_->ShouldShowWindowTitle()));

  // TODO(estade): to match classic Ash, this property should be toggled to true
  // for non-popups after the window is initially shown.
  properties[ash::mojom::kWindowPositionManaged_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int64_t>(
          !browser->bounds_overridden() && !browser->is_session_restore() &&
          !browser->is_type_popup()));
  properties[ash::mojom::kCanConsumeSystemKeys_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(browser->is_app()));
  // Set the frame color for WebUI windows, e.g. settings.
  if (!browser->is_type_tabbed() && browser->is_trusted_source()) {
    properties[ui::mojom::WindowManager::kFrameActiveColor_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int64_t>(BrowserFrameAsh::kMdWebUiFrameColor));
    properties[ui::mojom::WindowManager::kFrameInactiveColor_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int64_t>(BrowserFrameAsh::kMdWebUiFrameColor));
  }

  aura::WindowTreeHostMusInitParams window_tree_host_init_params =
      aura::CreateInitParamsForTopLevel(
          views::MusClient::Get()->window_tree_client(), std::move(properties));
  std::unique_ptr<views::DesktopWindowTreeHostMus> desktop_window_tree_host =
      std::make_unique<views::DesktopWindowTreeHostMus>(
          std::move(window_tree_host_init_params), browser_frame_, this);
  // BrowserNonClientFrameViewMash::OnBoundsChanged() takes care of updating
  // the insets.
  desktop_window_tree_host->set_auto_update_client_area(false);
  SetDesktopWindowTreeHost(std::move(desktop_window_tree_host));
  return params;
}

bool BrowserFrameMash::UseCustomFrame() const {
  return true;
}

bool BrowserFrameMash::UsesNativeSystemMenu() const {
  return false;
}

bool BrowserFrameMash::ShouldSaveWindowPlacement() const {
  return nullptr == GetWidget()->GetNativeWindow()->GetProperty(
                        ash::kRestoreBoundsOverrideKey);
}

void BrowserFrameMash::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  DesktopNativeWidgetAura::GetWindowPlacement(bounds, show_state);

  gfx::Rect* override_bounds = GetWidget()->GetNativeWindow()->GetProperty(
      ash::kRestoreBoundsOverrideKey);
  if (override_bounds && !override_bounds->IsEmpty()) {
    *bounds = *override_bounds;
    *show_state =
        ash::ToWindowShowState(GetWidget()->GetNativeWindow()->GetProperty(
            ash::kRestoreWindowStateTypeOverrideKey));
  }

  // Session restore might be unable to correctly restore other states.
  // For the record, https://crbug.com/396272
  if (*show_state != ui::SHOW_STATE_MAXIMIZED &&
      *show_state != ui::SHOW_STATE_MINIMIZED) {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
}

content::KeyboardEventProcessingResult BrowserFrameMash::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserFrameMash::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

int BrowserFrameMash::GetMinimizeButtonOffset() const {
  return 0;
}
