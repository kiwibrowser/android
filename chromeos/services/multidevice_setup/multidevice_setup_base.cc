// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"

namespace chromeos {

namespace multidevice_setup {

MultiDeviceSetupBase::MultiDeviceSetupBase() = default;

MultiDeviceSetupBase::~MultiDeviceSetupBase() = default;

void MultiDeviceSetupBase::BindRequest(mojom::MultiDeviceSetupRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace multidevice_setup

}  // namespace chromeos
