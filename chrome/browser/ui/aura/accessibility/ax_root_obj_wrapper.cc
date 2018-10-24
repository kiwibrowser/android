// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/ax_root_obj_wrapper.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/common/channel_info.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_window_obj_wrapper.h"

AXRootObjWrapper::AXRootObjWrapper() : alert_window_(new aura::Window(NULL)) {
  alert_window_->Init(ui::LAYER_NOT_DRAWN);

  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->AddObserver(this);
}

AXRootObjWrapper::~AXRootObjWrapper() {
  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->RemoveObserver(this);

  if (alert_window_) {
    delete alert_window_;
    alert_window_ = NULL;
  }
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetAlertForText(
    const std::string& text) {
  alert_window_->SetTitle(base::UTF8ToUTF16((text)));
  views::AXWindowObjWrapper* window_obj =
      static_cast<views::AXWindowObjWrapper*>(
          views::AXAuraObjCache::GetInstance()->GetOrCreate(alert_window_));
  window_obj->set_is_alert(true);
  return window_obj;
}

bool AXRootObjWrapper::HasChild(views::AXAuraObjWrapper* child) {
  std::vector<views::AXAuraObjWrapper*> children;
  GetChildren(&children);
  return base::ContainsValue(children, child);
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetParent() {
  return NULL;
}

void AXRootObjWrapper::GetChildren(
    std::vector<views::AXAuraObjWrapper*>* out_children) {
  views::AXAuraObjCache::GetInstance()->GetTopLevelWindows(out_children);
  out_children->push_back(
      views::AXAuraObjCache::GetInstance()->GetOrCreate(alert_window_));
}

void AXRootObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = unique_id_.Get();
  out_node_data->role = ax::mojom::Role::kDesktop;
  out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kChromeChannel,
                                    chrome::GetChannelName());

  display::Screen* screen = display::Screen::GetScreen();
  if (!screen)
    return;

  const display::Display& display = screen->GetPrimaryDisplay();

  // Utilize the display bounds to figure out if this screen is in landscape or
  // portrait. We use this rather than |rotation| because some devices default
  // to landscape, some in portrait. Encode landscape as horizontal state,
  // portrait as vertical state.
  if (display.bounds().width() > display.bounds().height())
    out_node_data->AddState(ax::mojom::State::kHorizontal);
  else
    out_node_data->AddState(ax::mojom::State::kVertical);
}

const ui::AXUniqueId& AXRootObjWrapper::GetUniqueId() const {
  return unique_id_;
}

void AXRootObjWrapper::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  AutomationManagerAura::GetInstance()->OnEvent(
      this, ax::mojom::Event::kLocationChanged);
}
