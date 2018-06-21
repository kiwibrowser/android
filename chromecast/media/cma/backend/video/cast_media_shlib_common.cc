// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/cast_media_shlib.h"

#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/public/media/media_capabilities_shlib.h"

namespace chromecast {
namespace media {

bool MediaCapabilitiesShlib::IsSupportedAudioConfig(const AudioConfig& config) {
  switch (config.codec) {
    case kCodecPCM:
    case kCodecPCM_S16BE:
    case kCodecAAC:
    case kCodecMP3:
    case kCodecVorbis:
      return true;
    default:
      break;
  }
  return false;
}

void CastMediaShlib::AddLoopbackAudioObserver(LoopbackAudioObserver* observer) {
  StreamMixer::Get()->AddLoopbackAudioObserver(observer);
}

void CastMediaShlib::RemoveLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  StreamMixer::Get()->RemoveLoopbackAudioObserver(observer);
}

}  // namespace media
}  // namespace chromecast
