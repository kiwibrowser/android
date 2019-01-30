// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_

#include "base/files/file_path.h"

namespace chromecast {
namespace media {

class CastAudioJson {
 public:
  static base::FilePath GetFilePath();
  static base::FilePath GetFilePathForTuning();
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
