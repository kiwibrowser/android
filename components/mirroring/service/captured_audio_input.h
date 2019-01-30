// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_CAPTURED_AUDIO_INPUT_H_
#define COMPONENTS_MIRRORING_SERVICE_CAPTURED_AUDIO_INPUT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/mirroring/service/interface.h"
#include "media/audio/audio_input_ipc.h"
#include "media/mojo/interfaces/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mirroring {

// CapturedAudioInput handles the creation, initialization and control of an
// audio input stream created by Audio Service.
class CapturedAudioInput final : public media::AudioInputIPC,
                                 public AudioStreamCreatorClient,
                                 public media::mojom::AudioInputStreamClient {
 public:
  using StreamCreatorCallback =
      base::RepeatingCallback<void(AudioStreamCreatorClient* client,
                                   const media::AudioParameters& params,
                                   uint32_t total_segments)>;
  explicit CapturedAudioInput(StreamCreatorCallback callback);
  ~CapturedAudioInput() override;

 private:
  // media::AudioInputIPC implementation.
  void CreateStream(media::AudioInputIPCDelegate* delegate,
                    const media::AudioParameters& params,
                    bool automatic_gain_control,
                    uint32_t total_segments) override;
  void RecordStream() override;
  void SetVolume(double volume) override;
  void CloseStream() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // AudioStreamCreatorClient implementation
  void StreamCreated(media::mojom::AudioInputStreamPtr stream,
                     media::mojom::AudioInputStreamClientRequest client_request,
                     media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
                     bool initially_muted) override;

  // media::mojom::AudioInputStreamClient implementation.
  void OnError() override;
  void OnMutedStateChanged(bool is_muted) override;

  SEQUENCE_CHECKER(sequence_checker_);

  const StreamCreatorCallback stream_creator_callback_;
  mojo::Binding<media::mojom::AudioInputStreamClient> stream_client_binding_;
  media::AudioInputIPCDelegate* delegate_ = nullptr;
  media::mojom::AudioInputStreamPtr stream_;

  DISALLOW_COPY_AND_ASSIGN(CapturedAudioInput);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_CAPTURED_AUDIO_INPUT_H_
