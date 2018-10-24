// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

void SendCdmAvailableUMA(const std::string& key_system, bool available) {
  base::UmaHistogramBoolean("Media.EME." +
                                media::GetKeySystemNameForUMA(key_system) +
                                ".LibraryCdmAvailable",
                            available);
}

std::vector<media::VideoCodec> GetEnabledHardwareSecureCodecsFromCommandLine() {
  std::vector<media::VideoCodec> result;

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line)
    return result;

  auto codecs_string = command_line->GetSwitchValueASCII(
      switches::kEnableHardwareSecureCodecsForTesting);
  const auto supported_codecs = base::SplitStringPiece(
      codecs_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const auto& codec : supported_codecs) {
    if (codec == "vp8")
      result.push_back(media::VideoCodec::kCodecVP8);
    else if (codec == "vp9")
      result.push_back(media::VideoCodec::kCodecVP9);
    else if (codec == "avc1")
      result.push_back(media::VideoCodec::kCodecH264);
    else
      DVLOG(1) << "Unsupported codec specified on command line: " << codec;
  }

  return result;
}

}  // namespace

// static
void KeySystemSupportImpl::Create(
    media::mojom::KeySystemSupportRequest request) {
  DVLOG(3) << __func__;
  // The created object is bound to (and owned by) |request|.
  mojo::MakeStrongBinding(std::make_unique<KeySystemSupportImpl>(),
                          std::move(request));
}

// static
std::unique_ptr<CdmInfo> KeySystemSupportImpl::GetCdmInfoForKeySystem(
    const std::string& key_system) {
  DVLOG(2) << __func__ << ": key_system = " << key_system;
  for (const auto& cdm : CdmRegistry::GetInstance()->GetAllRegisteredCdms()) {
    if (cdm.supported_key_system == key_system ||
        (cdm.supports_sub_key_systems &&
         media::IsChildKeySystemOf(key_system, cdm.supported_key_system))) {
      return std::make_unique<CdmInfo>(cdm);
    }
  }

  return nullptr;
}

KeySystemSupportImpl::KeySystemSupportImpl() = default;

KeySystemSupportImpl::~KeySystemSupportImpl() = default;

void KeySystemSupportImpl::IsKeySystemSupported(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DVLOG(3) << __func__ << ": key_system = " << key_system;

  auto cdm_info = GetCdmInfoForKeySystem(key_system);
  if (!cdm_info) {
    SendCdmAvailableUMA(key_system, false);
    std::move(callback).Run(false, nullptr);
    return;
  }

  SendCdmAvailableUMA(key_system, true);

  // Supported codecs and encryption schemes.
  auto capability = media::mojom::KeySystemCapability::New();
  const auto& schemes = cdm_info->supported_encryption_schemes;
  capability->video_codecs = cdm_info->supported_video_codecs;
  capability->encryption_schemes =
      std::vector<media::EncryptionMode>(schemes.begin(), schemes.end());

  if (base::FeatureList::IsEnabled(media::kHardwareSecureDecryption)) {
    capability->hw_secure_video_codecs =
        GetEnabledHardwareSecureCodecsFromCommandLine();

    // TODO(xhwang): Call into GetContentClient()->browser() to get key system
    // specific hardware secure decryption capability on Windows.
    NOTIMPLEMENTED();
  }

  // Temporary session is always supported.
  // TODO(xhwang): Populate this from CdmInfo.
  capability->session_types.push_back(media::CdmSessionType::TEMPORARY_SESSION);

  if (cdm_info->supports_persistent_license) {
    capability->session_types.push_back(
        media::CdmSessionType::PERSISTENT_LICENSE_SESSION);
  }

  std::move(callback).Run(true, std::move(capability));
}

}  // namespace content
