// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_provider_impl.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

namespace {

// This format should match //c/b/c/assistant/platform_audio_input_host.cc.
constexpr assistant_client::BufferFormat kFormat{
    16000 /* sample_rate */, assistant_client::INTERLEAVED_S32, 1 /* channels */
};

}  // namespace

AudioInputBufferImpl::AudioInputBufferImpl(const void* data,
                                           uint32_t frame_count)
    : data_(data), frame_count_(frame_count) {}

AudioInputBufferImpl::~AudioInputBufferImpl() = default;

assistant_client::BufferFormat AudioInputBufferImpl::GetFormat() const {
  return kFormat;
}

const void* AudioInputBufferImpl::GetData() const {
  return data_;
}

void* AudioInputBufferImpl::GetWritableData() {
  NOTREACHED();
  return nullptr;
}

int AudioInputBufferImpl::GetFrameCount() const {
  return frame_count_;
}

AudioInputImpl::AudioInputImpl(
    std::unique_ptr<service_manager::Connector> connector)
    : source_(audio::CreateInputDevice(
          std::move(connector),
          media::AudioDeviceDescription::kDefaultDeviceId)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(observer_sequence_checker_);
  // AUDIO_PCM_LINEAR and AUDIO_PCM_LOW_LATENCY are the same on CRAS.
  source_->Initialize(
      media::AudioParameters(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          media::CHANNEL_LAYOUT_MONO, kFormat.sample_rate,
          kFormat.sample_rate / 10 /* buffer size for 100 ms */),
      this);
}

AudioInputImpl::~AudioInputImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  source_->Stop();
}

void AudioInputImpl::Capture(const media::AudioBus* audio_source,
                             int audio_delay_milliseconds,
                             double volume,
                             bool key_pressed) {
  DCHECK_EQ(kFormat.num_channels, audio_source->channels());
  std::vector<int32_t> buffer(kFormat.num_channels * audio_source->frames());
  audio_source->ToInterleaved<media::SignedInt32SampleTypeTraits>(
      audio_source->frames(), buffer.data());
  int64_t time = base::TimeTicks::Now().since_origin().InMilliseconds() -
                 audio_delay_milliseconds;
  AudioInputBufferImpl input_buffer(buffer.data(), audio_source->frames());
  {
    base::AutoLock lock(lock_);
    for (auto* observer : observers_)
      observer->OnBufferAvailable(input_buffer, time);
  }
}

void AudioInputImpl::OnCaptureError(const std::string& message) {
  DLOG(ERROR) << "Capture error " << message;
  base::AutoLock lock(lock_);
  for (auto* observer : observers_)
    observer->OnError(AudioInput::Error::FATAL_ERROR);
}

void AudioInputImpl::OnCaptureMuted(bool is_muted) {}

assistant_client::BufferFormat AudioInputImpl::GetFormat() const {
  return kFormat;
}

void AudioInputImpl::AddObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  bool should_start = false;
  {
    base::AutoLock lock(lock_);
    observers_.push_back(observer);
    should_start = observers_.size() == 1;
  }

  if (should_start) {
    // Post to main thread runner to start audio recording. Assistant thread
    // does not have thread context defined in //base and will fail sequence
    // check in AudioCapturerSource::Start().
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StartRecording,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioInputImpl::RemoveObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  bool should_stop = false;
  {
    base::AutoLock lock(lock_);
    base::Erase(observers_, observer);
    should_stop = observers_.empty();
  }
  if (should_stop) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StopRecording,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioInputImpl::StartRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  source_->Start();
}

void AudioInputImpl::StopRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  source_->Stop();
}

AudioInputProviderImpl::AudioInputProviderImpl(
    service_manager::Connector* connector)
    : audio_input_(connector->Clone()) {}

AudioInputProviderImpl::~AudioInputProviderImpl() = default;

assistant_client::AudioInput& AudioInputProviderImpl::GetAudioInput() {
  return audio_input_;
}

int64_t AudioInputProviderImpl::GetCurrentAudioTime() {
  // TODO(xiaohuic): see if we can support real timestamp.
  return 0;
}

}  // namespace assistant
}  // namespace chromeos
