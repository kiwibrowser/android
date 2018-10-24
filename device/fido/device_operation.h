// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DEVICE_OPERATION_H_
#define DEVICE_FIDO_DEVICE_OPERATION_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"

namespace device {

template <class Request, class Response>
class DeviceOperation {
 public:
  using DeviceResponseCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<Response>)>;
  // Represents a per device logic that is owned by FidoTask. Thus,
  // DeviceOperation does not outlive |request|.
  DeviceOperation(FidoDevice* device,
                  const Request& request,
                  DeviceResponseCallback callback)
      : device_(device), request_(request), callback_(std::move(callback)) {}

  virtual ~DeviceOperation() = default;

  virtual void Start() = 0;

 protected:
  // TODO(hongjunchoi): Refactor so that |command| is never base::nullopt.
  void DispatchDeviceRequest(base::Optional<std::vector<uint8_t>> command,
                             FidoDevice::DeviceCallback callback) {
    if (!command) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    device_->DeviceTransact(std::move(*command), std::move(callback));
  }

  const Request& request() const { return request_; }
  FidoDevice* device() const { return device_; }
  DeviceResponseCallback callback() { return std::move(callback_); }

 private:
  FidoDevice* const device_ = nullptr;
  const Request& request_;
  DeviceResponseCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOperation);
};

}  // namespace device

#endif  // DEVICE_FIDO_DEVICE_OPERATION_H_
