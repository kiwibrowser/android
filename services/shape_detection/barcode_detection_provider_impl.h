// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_IMPL_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_IMPL_H_

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

namespace shape_detection {

class BarcodeDetectionProviderImpl
    : public shape_detection::mojom::BarcodeDetectionProvider {
 public:
  ~BarcodeDetectionProviderImpl() override = default;

  static void Create(
      shape_detection::mojom::BarcodeDetectionProviderRequest request) {
    mojo::MakeStrongBinding(std::make_unique<BarcodeDetectionProviderImpl>(),
                            std::move(request));
  }

  void CreateBarcodeDetection(
      shape_detection::mojom::BarcodeDetectionRequest request,
      shape_detection::mojom::BarcodeDetectorOptionsPtr options) override;
  void EnumerateSupportedFormats(
      EnumerateSupportedFormatsCallback callback) override;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_IMPL_H_
