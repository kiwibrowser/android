// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_provider_mac.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/barcode_detection_impl_mac.h"

namespace shape_detection {

BarcodeDetectionProviderMac::BarcodeDetectionProviderMac() = default;

BarcodeDetectionProviderMac::~BarcodeDetectionProviderMac() = default;

// static
void BarcodeDetectionProviderMac::Create(
    mojom::BarcodeDetectionProviderRequest request) {
  mojo::MakeStrongBinding(std::make_unique<BarcodeDetectionProviderMac>(),
                          std::move(request));
}

void BarcodeDetectionProviderMac::CreateBarcodeDetection(
    mojom::BarcodeDetectionRequest request,
    mojom::BarcodeDetectorOptionsPtr options) {
  // Barcode detection needs at least MAC OS X 10.10.
  if (@available(macOS 10.10, *)) {
    mojo::MakeStrongBinding(std::make_unique<BarcodeDetectionImplMac>(),
                            std::move(request));
  }
}

void BarcodeDetectionProviderMac::EnumerateSupportedFormats(
    EnumerateSupportedFormatsCallback callback) {
  // Barcode detection needs at least MAC OS X 10.10.
  if (@available(macOS 10.10, *)) {
    // Mac implementation supports only one BarcodeFormat.
    std::move(callback).Run({mojom::BarcodeFormat::QR_CODE});
    return;
  }

  DLOG(ERROR) << "Platform not supported for Barcode Detection.";
  std::move(callback).Run({});
}

}  // namespace shape_detection
