// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/captured_audio_input.h"

#include "base/logging.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mirroring {

CapturedAudioInput::CapturedAudioInput(StreamCreatorCallback callback)
    : stream_creator_callback_(std::move(callback)),
      stream_client_binding_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!stream_creator_callback_.is_null());
}

CapturedAudioInput::~CapturedAudioInput() {}

void CapturedAudioInput::CreateStream(media::AudioInputIPCDelegate* delegate,
                                      const media::AudioParameters& params,
                                      bool automatic_gain_control,
                                      uint32_t total_segments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!automatic_gain_control);  // Invalid to be true for screen capture.
  DCHECK(delegate);
  DCHECK(!delegate_);
  delegate_ = delegate;
  stream_creator_callback_.Run(this, params, total_segments);
}

void CapturedAudioInput::RecordStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->Record();
}

void CapturedAudioInput::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->SetVolume(volume);
}

void CapturedAudioInput::CloseStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = nullptr;
  if (stream_client_binding_.is_bound())
    stream_client_binding_.Unbind();
  stream_.reset();
}

void CapturedAudioInput::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  NOTREACHED();
}

void CapturedAudioInput::StreamCreated(
    media::mojom::AudioInputStreamPtr stream,
    media::mojom::AudioInputStreamClientRequest client_request,
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  DCHECK(!stream_);
  DCHECK(!stream_client_binding_.is_bound());

  stream_ = std::move(stream);
  stream_client_binding_.Bind(std::move(client_request));

  base::PlatformFile socket_handle;
  auto result =
      mojo::UnwrapPlatformFile(std::move(data_pipe->socket), &socket_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);

  base::ReadOnlySharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  delegate_->OnStreamCreated(std::move(shared_memory_region), socket_handle,
                             initially_muted);
}

void CapturedAudioInput::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnError();
}

void CapturedAudioInput::OnMutedStateChanged(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnMuted(is_muted);
}

}  // namespace mirroring
