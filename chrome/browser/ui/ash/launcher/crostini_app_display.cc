// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/crostini_app_display.h"

#include "ui/display/types/display_constants.h"

CrostiniAppDisplay::CrostiniAppDisplay() = default;

CrostiniAppDisplay::~CrostiniAppDisplay() = default;

void CrostiniAppDisplay::Register(const std::string& startup_id,
                                  int64_t display_id) {
  while (startup_ids_.size() >= kMaxStartupIdSize) {
    startup_id_to_display_id_.erase(startup_ids_.front());
    startup_ids_.pop_front();
  }
  auto it = startup_id_to_display_id_.find(startup_id);
  if (it == startup_id_to_display_id_.end()) {
    startup_id_to_display_id_.emplace(std::make_pair(startup_id, display_id));
    startup_ids_.push_back(startup_id);
  } else {
    it->second = display_id;
  }
}

int64_t CrostiniAppDisplay::GetDisplayIdForStartupId(
    const std::string& startup_id) {
  auto it = startup_id_to_display_id_.find(startup_id);
  if (it == startup_id_to_display_id_.end()) {
    return display::kInvalidDisplayId;
  } else {
    return it->second;
  }
}
