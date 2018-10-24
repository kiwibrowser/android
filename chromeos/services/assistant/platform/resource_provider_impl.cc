// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/resource_provider_impl.h"

#include "chromeos/grit/chromeos_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

constexpr char kHotwordModelName[] = "ok google";

}  // namespace

namespace chromeos {
namespace assistant {

ResourceProviderImpl::ResourceProviderImpl() = default;

ResourceProviderImpl::~ResourceProviderImpl() = default;

bool ResourceProviderImpl::GetResource(uint16_t resource_id, std::string* out) {
  int chrome_resource_id = -1;
  switch (resource_id) {
    case assistant_client::resource_ids::kGeneralError:
      chrome_resource_id = IDR_ASSISTANT_SPEECH_RECOGNITION_ERROR;
      break;
    case assistant_client::resource_ids::kWifiNeedsSetupError:
    case assistant_client::resource_ids::kWifiNotConnectedError:
    case assistant_client::resource_ids::kWifiCannotConnectError:
    case assistant_client::resource_ids::kNetworkConnectingError:
    // These above do not apply to ChromeOS, but let it fall through to get a
    // generic error.
    case assistant_client::resource_ids::kNetworkCannotReachServerError:
      chrome_resource_id = IDR_ASSISTANT_NO_INTERNET_ERROR;
      break;
    case assistant_client::resource_ids::kDefaultHotwordResourceId:
      return GetHotwordData(GetDefaultHotwordName(), out);
    default:
      break;
  }

  if (chrome_resource_id < 0)
    return false;

  auto data = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
      chrome_resource_id);
  out->assign(data.data(), data.length());
  return true;
}

std::vector<std::string> ResourceProviderImpl::GetHotwordNameList() {
  std::vector<std::string> result;
  std::string name = GetDefaultHotwordName();
  if (!name.empty())
    result.push_back(name);
  return result;
}

std::string ResourceProviderImpl::GetDefaultHotwordName() {
  return kHotwordModelName;
}

bool ResourceProviderImpl::GetHotwordData(const std::string& name,
                                          std::string* result) {
  if (name != kHotwordModelName)
    return false;

  auto data = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_ASSISTANT_HOTWORD_MODEL);
  result->assign(data.data(), data.length());
  return true;
}

}  // namespace assistant
}  // namespace chromeos
