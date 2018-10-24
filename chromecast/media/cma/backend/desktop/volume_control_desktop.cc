// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/volume_control.h"

#include <string>
#include <vector>

namespace chromecast {
namespace media {

// static
void VolumeControl::Initialize(const std::vector<std::string>& argv) {}

// static
void VolumeControl::Finalize() {}

// static
void VolumeControl::AddVolumeObserver(VolumeObserver* observer) {}

// static
void VolumeControl::RemoveVolumeObserver(VolumeObserver* observer) {}

// static
float VolumeControl::GetVolume(AudioContentType type) {
  return 1.0f;
}

// static
void VolumeControl::SetVolume(AudioContentType type, float level) {}

// static
bool VolumeControl::IsMuted(AudioContentType type) {
  return false;
}

// static
void VolumeControl::SetMuted(AudioContentType type, bool muted) {}

// static
void VolumeControl::SetOutputLimit(AudioContentType type, float limit) {}

// static
float VolumeControl::VolumeToDbFS(float volume) {
  return 0.0f;
}

// static
float VolumeControl::DbFSToVolume(float db) {
  return 0.0f;
}

// static
void VolumeControl::SetPowerSaveMode(bool power_save_on) {}

}  // namespace media
}  // namespace chromecast
