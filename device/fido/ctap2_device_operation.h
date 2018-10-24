// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_
#define DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/device_operation.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"

namespace device {

// Represents per device logic for CTAP2 authenticators. Ctap2DeviceOperation
// is owned by FidoTask, and thus |request| outlives Ctap2DeviceOperation.
template <class Request, class Response>
class Ctap2DeviceOperation : public DeviceOperation<Request, Response> {
 public:
  using DeviceResponseCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<Response>)>;
  using DeviceResponseParser =
      base::OnceCallback<base::Optional<Response>(base::span<const uint8_t>)>;

  Ctap2DeviceOperation(FidoDevice* device,
                       const Request& request,
                       DeviceResponseCallback callback,
                       DeviceResponseParser device_response_parser)
      : DeviceOperation<Request, Response>(device,
                                           request,
                                           std::move(callback)),
        device_response_parser_(std::move(device_response_parser)),
        weak_factory_(this) {}

  ~Ctap2DeviceOperation() override = default;

  void Start() override {
    this->device()->DeviceTransact(
        this->request().EncodeAsCBOR(),
        base::BindOnce(&Ctap2DeviceOperation::OnResponseReceived,
                       weak_factory_.GetWeakPtr()));
  }

  void OnResponseReceived(
      base::Optional<std::vector<uint8_t>> device_response) {
    if (!device_response) {
      std::move(this->callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
      return;
    }

    const auto response_code = GetResponseCode(*device_response);
    std::move(this->callback())
        .Run(response_code, std::move(device_response_parser_)
                                .Run(std::move(*device_response)));
  }

 private:
  DeviceResponseParser device_response_parser_;
  base::WeakPtrFactory<Ctap2DeviceOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Ctap2DeviceOperation);
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_
