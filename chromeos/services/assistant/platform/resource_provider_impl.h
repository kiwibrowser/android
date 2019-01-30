// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_RESOURCE_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_RESOURCE_PROVIDER_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "libassistant/shared/public/platform_resources.h"

namespace chromeos {
namespace assistant {

class ResourceProviderImpl : public assistant_client::ResourceProvider {
 public:
  ResourceProviderImpl();
  ~ResourceProviderImpl() override;

  // assistant_client::ResourceProvider implementation:
  bool GetResource(uint16_t resource_id, std::string* out) override;
  std::vector<std::string> GetHotwordNameList() override;
  std::string GetDefaultHotwordName() override;
  bool GetHotwordData(const std::string& name, std::string* result) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_RESOURCE_PROVIDER_IMPL_H_
