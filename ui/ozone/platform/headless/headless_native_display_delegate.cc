// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_native_display_delegate.h"

#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"

namespace ui {

namespace {
constexpr gfx::Size kDefaultWindowSize(800, 600);
constexpr int default_refresh = 60;
}  // namespace

HeadlessNativeDisplayDelegate::HeadlessNativeDisplayDelegate() = default;

HeadlessNativeDisplayDelegate::~HeadlessNativeDisplayDelegate() = default;

void HeadlessNativeDisplayDelegate::Initialize() {
  // This shouldn't be called twice.
  DCHECK(!current_snapshot_);

  if (next_display_id_ == std::numeric_limits<int64_t>::max()) {
    LOG(FATAL) << "Exceeded display id limit";
    return;
  }

  current_snapshot_.reset(new display::DisplaySnapshot(
      get_next_display_id(), gfx::Point(0, 0), kDefaultWindowSize,
      display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE, false,
      false, false, gfx::ColorSpace(), "", base::FilePath(),
      display::DisplaySnapshot::DisplayModeList(), std::vector<uint8_t>(),
      nullptr, nullptr, 0, 0, gfx::Size()));

  current_mode_.reset(
      new display::DisplayMode(kDefaultWindowSize, false, default_refresh));
  current_snapshot_->set_current_mode(current_mode_.get());

  for (display::NativeDisplayObserver& observer : observers_)
    observer.OnConfigurationChanged();
}

void HeadlessNativeDisplayDelegate::TakeDisplayControl(
    display::DisplayControlCallback callback) {
  NOTREACHED();
}

void HeadlessNativeDisplayDelegate::RelinquishDisplayControl(
    display::DisplayControlCallback callback) {
  NOTREACHED();
}

void HeadlessNativeDisplayDelegate::GetDisplays(
    display::GetDisplaysCallback callback) {
  std::vector<display::DisplaySnapshot*> snapshot;
  snapshot.push_back(current_snapshot_.get());
  std::move(callback).Run(snapshot);
}

void HeadlessNativeDisplayDelegate::Configure(
    const display::DisplaySnapshot& output,
    const display::DisplayMode* mode,
    const gfx::Point& origin,
    display::ConfigureCallback callback) {
  NOTREACHED();
}

void HeadlessNativeDisplayDelegate::GetHDCPState(
    const display::DisplaySnapshot& output,
    display::GetHDCPStateCallback callback) {
  NOTREACHED();
}

void HeadlessNativeDisplayDelegate::SetHDCPState(
    const display::DisplaySnapshot& output,
    display::HDCPState state,
    display::SetHDCPStateCallback callback) {
  NOTREACHED();
}

bool HeadlessNativeDisplayDelegate::SetColorMatrix(
    int64_t display_id,
    const std::vector<float>& color_matrix) {
  NOTREACHED();
  return false;
}

bool HeadlessNativeDisplayDelegate::SetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  NOTREACHED();
  return false;
}

void HeadlessNativeDisplayDelegate::AddObserver(
    display::NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void HeadlessNativeDisplayDelegate::RemoveObserver(
    display::NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

display::FakeDisplayController*
HeadlessNativeDisplayDelegate::GetFakeDisplayController() {
  return nullptr;
}

}  // namespace ui
