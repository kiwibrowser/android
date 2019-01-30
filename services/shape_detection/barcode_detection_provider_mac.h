// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_MAC_H_
#define SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_MAC_H_

#include "base/macros.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

namespace shape_detection {

// The BarcodeDetectionProviderMac class is a provider that binds an
// implementation of mojom::BarcodeDetection with Core Image or Vision
// Framework.
class BarcodeDetectionProviderMac
    : public shape_detection::mojom::BarcodeDetectionProvider {
 public:
  BarcodeDetectionProviderMac();
  ~BarcodeDetectionProviderMac() override;

  // Binds BarcodeDetection provider request to the implementation of
  // mojom::BarcodeDetectionProvider.
  static void Create(mojom::BarcodeDetectionProviderRequest request);

  void CreateBarcodeDetection(
      mojom::BarcodeDetectionRequest request,
      mojom::BarcodeDetectorOptionsPtr options) override;
  void EnumerateSupportedFormats(
      EnumerateSupportedFormatsCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BarcodeDetectionProviderMac);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_BARCODE_DETECTION_PROVIDER_MAC_H_
