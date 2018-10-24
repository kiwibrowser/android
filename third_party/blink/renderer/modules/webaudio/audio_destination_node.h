/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_DESTINATION_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_DESTINATION_NODE_H_

#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_io_callback.h"

namespace blink {

class AudioBus;
class BaseAudioContext;

class AudioDestinationHandler : public AudioHandler, public AudioIOCallback {
 public:
  AudioDestinationHandler(AudioNode&);
  ~AudioDestinationHandler() override;

  // AudioHandler
  void Process(size_t) final {
  }  // we're pulled by hardware so this is never called

  // Invoked by the AudioDestination to get the next render quantum into
  // |destination_bus|.
  void Render(AudioBus* destination_bus,
              size_t number_of_frames,
              const AudioIOPosition& output_position) final;

  size_t CurrentSampleFrame() const {
    return AcquireLoad(&current_sample_frame_);
  }
  double CurrentTime() const {
    return CurrentSampleFrame() / static_cast<double>(SampleRate());
  }

  virtual unsigned long MaxChannelCount() const { return 0; }

  virtual void StartRendering() = 0;
  virtual void StopRendering() = 0;

  // The render thread needs to be changed after Worklet JS code is loaded by
  // AudioWorklet. This method ensures the switching of render thread and the
  // restart of the context.
  virtual void RestartRendering() = 0;

  // Returns the rendering callback buffer size.
  size_t CallbackBufferSize() const override = 0;
  virtual double SampleRate() const = 0;

  // Returns the audio buffer size in frames used by the AudioContext.
  virtual int FramesPerBuffer() const = 0;

 protected:
  // Counts the number of sample-frames processed by the destination.
  size_t current_sample_frame_;
};

class AudioDestinationNode : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AudioDestinationHandler& GetAudioDestinationHandler() const;

  unsigned long maxChannelCount() const;
  size_t CallbackBufferSize() const { return Handler().CallbackBufferSize(); }

 protected:
  AudioDestinationNode(BaseAudioContext&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_DESTINATION_NODE_H_
