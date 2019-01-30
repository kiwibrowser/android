// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/gbm_device_linux.h"

#include <gbm.h>

namespace ui {

GbmDeviceLinux::GbmDeviceLinux() {}

GbmDeviceLinux::~GbmDeviceLinux() {
  if (device_)
    gbm_device_destroy(device_);
}

bool GbmDeviceLinux::InitializeGbmDevice(int fd) {
  device_ = gbm_create_device(fd);
  return !!device_;
}

}  // namespace ui
