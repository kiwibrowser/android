// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fuchsia/audio_manager_fuchsia.h"

#include <memory>

#include <media/audio.h>

#include "media/audio/fuchsia/audio_output_stream_fuchsia.h"

namespace media {

AudioManagerFuchsia::AudioManagerFuchsia(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

AudioManagerFuchsia::~AudioManagerFuchsia() = default;

bool AudioManagerFuchsia::HasAudioOutputDevices() {
  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API for device
  // enumeration. Update this method when that functionality is implemented.
  return true;
}

bool AudioManagerFuchsia::HasAudioInputDevices() {
  NOTIMPLEMENTED();
  return false;
}

void AudioManagerFuchsia::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  device_names->clear();
  NOTIMPLEMENTED();
}

void AudioManagerFuchsia::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API for device
  // enumeration. Update this method when that functionality is implemented.
  *device_names = {AudioDeviceName::CreateDefault()};
}

AudioParameters AudioManagerFuchsia::GetInputStreamParameters(
    const std::string& device_id) {
  NOTREACHED();
  return AudioParameters();
}

AudioParameters AudioManagerFuchsia::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API to get
  // device configuration. Update this method when that functionality is
  // implemented.
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 480);
}

const char* AudioManagerFuchsia::GetName() {
  return "Fuchsia";
}

AudioOutputStream* AudioManagerFuchsia::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioOutputStream* AudioManagerFuchsia::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (!device_id.empty() &&
      device_id != AudioDeviceDescription::kDefaultDeviceId) {
    return nullptr;
  }

  return new AudioOutputStreamFuchsia(this, params);
}

AudioInputStream* AudioManagerFuchsia::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* AudioManagerFuchsia::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerFuchsia>(std::move(audio_thread),
                                               audio_log_factory);
}

}  // namespace media
