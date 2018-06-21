// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/output_device.h"

#include <utility>

#include "media/audio/audio_output_device_thread_callback.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/public/mojom/constants.mojom.h"

namespace audio {

OutputDevice::OutputDevice(
    std::unique_ptr<service_manager::Connector> connector,
    const media::AudioParameters& params,
    media::AudioRendererSink::RenderCallback* render_callback,
    const std::string& device_id)
    : audio_parameters_(params),
      render_callback_(render_callback),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(params.IsValid());
  connector->BindInterface(audio::mojom::kServiceName,
                           mojo::MakeRequest(&stream_factory_));

  media::mojom::AudioOutputStreamRequest stream_request =
      mojo::MakeRequest(&stream_);
  stream_.set_connection_error_handler(
      base::BindOnce(&OutputDevice::CleanUp, weak_factory_.GetWeakPtr()));
  stream_factory_->CreateOutputStream(
      std::move(stream_request), nullptr, nullptr, device_id, params,
      base::UnguessableToken::Create(),
      base::BindOnce(&OutputDevice::StreamCreated, weak_factory_.GetWeakPtr()));
}

OutputDevice::~OutputDevice() {
  CleanUp();
}

void OutputDevice::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Play();
}

void OutputDevice::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Pause();
}

void OutputDevice::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->SetVolume(volume);
}

void OutputDevice::StreamCreated(
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data_pipe)
    return;

  base::PlatformFile socket_handle;
  auto result =
      mojo::UnwrapPlatformFile(std::move(data_pipe->socket), &socket_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  base::UnsafeSharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  audio_callback_.reset(new media::AudioOutputDeviceThreadCallback(
      audio_parameters_, std::move(shared_memory_region), render_callback_));
  audio_thread_.reset(new media::AudioDeviceThread(
      audio_callback_.get(), socket_handle, "audio::OutputDevice",
      base::ThreadPriority::REALTIME_AUDIO));
}

void OutputDevice::CleanUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_thread_.reset();
  audio_callback_.reset();
  stream_.reset();
  stream_factory_.reset();
}

}  // namespace audio
