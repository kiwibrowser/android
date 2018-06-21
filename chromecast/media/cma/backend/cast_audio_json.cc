// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/cast_audio_json.h"

#include "base/files/file_util.h"
#include "build/build_config.h"

namespace chromecast {
namespace media {

#if defined(OS_FUCHSIA)
const char kCastAudioJsonFilePath[] = "/system/data/cast_audio.json";
#else
const char kCastAudioJsonFilePath[] = "/etc/cast_audio.json";
#endif
const char kCastAudioJsonFileName[] = "cast_audio.json";

// static
base::FilePath CastAudioJson::GetFilePath() {
  base::FilePath tuning_path = CastAudioJson::GetFilePathForTuning();
  if (base::PathExists(tuning_path)) {
    return tuning_path;
  }

  return base::FilePath(kCastAudioJsonFilePath);
}

// static
base::FilePath CastAudioJson::GetFilePathForTuning() {
  return base::GetHomeDir().Append(kCastAudioJsonFileName);
}

}  // namespace media
}  // namespace chromecast
