// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_properties.h"

namespace media {

SupportedCodecs KeySystemProperties::GetSupportedHwSecureCodecs() const {
  return EME_CODEC_NONE;
}

bool KeySystemProperties::UseAesDecryptor() const {
  return false;
}

}  // namespace media
