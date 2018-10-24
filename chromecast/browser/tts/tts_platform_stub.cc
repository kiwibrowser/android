// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/tts/tts_platform_stub.h"

namespace chromecast {

bool TtsPlatformImplStub::PlatformImplAvailable() {
  return true;
}

bool TtsPlatformImplStub::Speak(int utterance_id,
                                const std::string& utterance,
                                const std::string& lang,
                                const VoiceData& voice,
                                const UtteranceContinuousParameters& params) {
  LOG(INFO) << "Speak: " << utterance;
  return true;
}

bool TtsPlatformImplStub::StopSpeaking() {
  LOG(INFO) << "StopSpeaking";
  return true;
}

void TtsPlatformImplStub::Pause() {
  LOG(INFO) << "Pause";
}

void TtsPlatformImplStub::Resume() {
  LOG(INFO) << "Resume";
}

bool TtsPlatformImplStub::IsSpeaking() {
  LOG(INFO) << "IsSpeaking";
  return false;
}

void TtsPlatformImplStub::GetVoices(std::vector<VoiceData>* out_voices) {
  LOG(INFO) << "GetVoices";
}

}  // namespace chromecast
