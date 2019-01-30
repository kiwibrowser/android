// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_CORRECTION_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_CORRECTION_TEST_UTILS_H_

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

enum PixelsAlphaMultiply {
  kAlphaMultiplied,
  kAlphaUnmultiplied,
};

enum UnpremulRoundTripTolerance {
  kNoUnpremulRoundTripTolerance,
  kUnpremulRoundTripTolerance,
};

class ColorCorrectionTestUtils {
 public:
  static void CompareColorCorrectedPixels(
      const void* actual_pixels,
      const void* expected_pixels,
      int num_pixels,
      ImageDataStorageFormat src_storage_format,
      PixelsAlphaMultiply alpha_multiplied,
      UnpremulRoundTripTolerance premul_unpremul_tolerance);

  static bool ConvertPixelsToColorSpaceAndPixelFormatForTest(
      void* src_data,
      int num_elements,
      CanvasColorSpace src_color_space,
      ImageDataStorageFormat src_storage_format,
      CanvasColorSpace dst_color_space,
      CanvasPixelFormat dst_pixel_format,
      std::unique_ptr<uint8_t[]>& converted_pixels,
      SkColorSpaceXform::ColorFormat color_format_for_f16_canvas);

  static bool MatchColorSpace(SkColorSpace* src_color_space,
                              SkColorSpace* dst_color_space,
                              float xyz_d50_component_tolerance);

  static bool MatchSkImages(sk_sp<SkImage> src_image,
                            sk_sp<SkImage> dst_image,
                            unsigned uint8_tolerance,
                            float f16_tolerance,
                            float xyz_d50_component_tolerance,
                            bool compare_alpha);

 private:
  static bool IsNearlyTheSame(float expected, float actual, float tolerance);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_CORRECTION_TEST_UTILS_H_
