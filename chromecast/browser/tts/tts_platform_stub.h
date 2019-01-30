// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/tts/tts_controller.h"
#include "chromecast/browser/tts/tts_platform.h"

#ifndef CHROMECAST_BROWSER_TTS_TTS_PLATFORM_STUB_H_
#define CHROMECAST_BROWSER_TTS_TTS_PLATFORM_STUB_H_

namespace chromecast {

// The default stub implementation of TtsPlaform for Cast that merely logs TTS
// events.
class TtsPlatformImplStub : public TtsPlatformImpl {
 public:
  TtsPlatformImplStub() = default;
  ~TtsPlatformImplStub() override = default;

  bool PlatformImplAvailable() override;

  bool Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params) override;

  bool StopSpeaking() override;

  void Pause() override;

  void Resume() override;

  bool IsSpeaking() override;

  void GetVoices(std::vector<VoiceData>* out_voices) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplStub);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TTS_TTS_PLATFORM_STUB_H_
