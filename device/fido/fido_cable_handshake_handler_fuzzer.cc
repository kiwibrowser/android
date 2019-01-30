// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "device/fido/fido_cable_device.h"
#include "device/fido/fido_cable_handshake_handler.h"
#include "device/fido/fido_constants.h"

namespace {

constexpr std::array<uint8_t, 32> kTestSessionPreKey = {{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
}};

constexpr std::array<uint8_t, 8> kTestNonce = {{
    0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08,
}};

constexpr char kTestDeviceAddress[] = "Fake_Address";

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* raw_data, size_t size) {
  auto data_span = base::make_span(raw_data, size);
  device::FidoCableDevice test_cable_device(kTestDeviceAddress);
  device::FidoCableHandshakeHandler handshake_handler(
      &test_cable_device, kTestNonce, kTestSessionPreKey);
  handshake_handler.ValidateAuthenticatorHandshakeMessage(data_span);
  return 0;
}
