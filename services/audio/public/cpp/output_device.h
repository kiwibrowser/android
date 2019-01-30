// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_OUTPUT_DEVICE_H_
#define SERVICES_AUDIO_PUBLIC_CPP_OUTPUT_DEVICE_H_

#include <memory>
#include <string>

#include "media/base/audio_renderer_sink.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace media {
class AudioDeviceThread;
class AudioOutputDeviceThreadCallback;
}  // namespace media

namespace audio {

class OutputDevice {
 public:
  OutputDevice(std::unique_ptr<service_manager::Connector> connector,
               const media::AudioParameters& params,
               media::AudioRendererSink::RenderCallback* callback,
               const std::string& device_id);
  ~OutputDevice();

  void Play();
  void Pause();
  void SetVolume(double volume);

 private:
  void StreamCreated(media::mojom::ReadWriteAudioDataPipePtr data_pipe);
  void CleanUp();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<media::AudioOutputDeviceThreadCallback> audio_callback_;
  std::unique_ptr<media::AudioDeviceThread> audio_thread_;
  media::AudioParameters audio_parameters_;
  media::AudioRendererSink::RenderCallback* render_callback_;
  media::mojom::AudioOutputStreamPtr stream_;
  audio::mojom::StreamFactoryPtr stream_factory_;

  base::WeakPtrFactory<OutputDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OutputDevice);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_OUTPUT_DEVICE_H_
