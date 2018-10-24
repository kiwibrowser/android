// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"

namespace blink {
namespace {

// Needed for ImageFrame::SetMemoryAllocator, but still does the default
// allocation.
class TestAllocator final : public SkBitmap::Allocator {
  bool allocPixelRef(SkBitmap* dst) override { return dst->tryAllocPixels(); }
};

class ImageFrameTest : public testing::Test {
 public:
  void SetUp() override {
    src_8888_a = 0x80;
    src_8888_r = 0x40;
    src_8888_g = 0x50;
    src_8888_b = 0x60;
    src_8888 = SkPackARGB32(src_8888_a, src_8888_r, src_8888_g, src_8888_b);
    dst_8888 = SkPackARGB32(0xA0, 0x60, 0x70, 0x80);

    typedef SkColorSpaceXform::ColorFormat ColorFormat;
    color_format_8888 = ColorFormat::kBGRA_8888_ColorFormat;
    if (kN32_SkColorType == kRGBA_8888_SkColorType)
      color_format_8888 = ColorFormat::kRGBA_8888_ColorFormat;
    color_format_f16 = ColorFormat::kRGBA_F16_ColorFormat;
    color_format_f32 = ColorFormat::kRGBA_F32_ColorFormat;

    sk_sp<SkColorSpace> srgb_linear = SkColorSpace::MakeSRGBLinear();
    SkColorSpaceXform::Apply(srgb_linear.get(), color_format_f16, &src_f16,
                             srgb_linear.get(), color_format_8888, &src_8888, 1,
                             SkColorSpaceXform::AlphaOp::kPreserve_AlphaOp);
    SkColorSpaceXform::Apply(srgb_linear.get(), color_format_f16, &dst_f16,
                             srgb_linear.get(), color_format_8888, &dst_8888, 1,
                             SkColorSpaceXform::AlphaOp::kPreserve_AlphaOp);
  }

 protected:
  const float color_compoenent_tolerance = 0.01;
  unsigned src_8888_a, src_8888_r, src_8888_g, src_8888_b;
  ImageFrame::PixelData src_8888, dst_8888;
  ImageFrame::PixelDataF16 src_f16, dst_f16;
  SkColorSpaceXform::ColorFormat color_format_8888, color_format_f16,
      color_format_f32;

  void ConvertN32ToF32(float* dst, ImageFrame::PixelData src) {
    sk_sp<SkColorSpace> srgb_linear = SkColorSpace::MakeSRGBLinear();
    SkColorSpaceXform::Apply(srgb_linear.get(), color_format_f32, dst,
                             srgb_linear.get(), color_format_8888, &src, 1,
                             SkColorSpaceXform::AlphaOp::kPreserve_AlphaOp);
  }

  void ConvertF16ToF32(float* dst, ImageFrame::PixelDataF16 src) {
    sk_sp<SkColorSpace> srgb_linear = SkColorSpace::MakeSRGBLinear();
    SkColorSpaceXform::Apply(srgb_linear.get(), color_format_f32, dst,
                             srgb_linear.get(), color_format_f16, &src, 1,
                             SkColorSpaceXform::AlphaOp::kPreserve_AlphaOp);
  }
};

TEST_F(ImageFrameTest, TestF16API) {
  ImageFrame::PixelFormat kN32 = ImageFrame::PixelFormat::kN32;
  ImageFrame::PixelFormat kRGBA_F16 = ImageFrame::PixelFormat::kRGBA_F16;

  ImageFrame frame_no_pixel_format;
  ASSERT_EQ(kN32, frame_no_pixel_format.GetPixelFormat());

  ImageFrame frame_pixel_format_n32(kN32);
  ASSERT_EQ(kN32, frame_pixel_format_n32.GetPixelFormat());

  ImageFrame frame_pixel_format_f16(kRGBA_F16);
  ASSERT_EQ(kRGBA_F16, frame_pixel_format_f16.GetPixelFormat());

  ImageFrame frame_copy_ctor_n32(frame_pixel_format_n32);
  ASSERT_EQ(kN32, frame_copy_ctor_n32.GetPixelFormat());

  ImageFrame frame_copy_ctor_f16(frame_pixel_format_f16);
  ASSERT_EQ(kRGBA_F16, frame_copy_ctor_f16.GetPixelFormat());

  ImageFrame frame_test_assignment;
  frame_test_assignment = frame_pixel_format_n32;
  ASSERT_EQ(kN32, frame_test_assignment.GetPixelFormat());
  frame_test_assignment = frame_pixel_format_f16;
  ASSERT_EQ(kRGBA_F16, frame_test_assignment.GetPixelFormat());

  SkBitmap bitmap(frame_pixel_format_f16.Bitmap());
  ASSERT_EQ(0, bitmap.width());
  ASSERT_EQ(0, bitmap.height());
  ASSERT_EQ(nullptr, bitmap.colorSpace());

  TestAllocator allocator;
  frame_pixel_format_f16.SetMemoryAllocator(&allocator);
  sk_sp<SkColorSpace> srgb_linear = SkColorSpace::MakeSRGBLinear();

  ASSERT_TRUE(frame_pixel_format_f16.AllocatePixelData(2, 2, srgb_linear));
  bitmap = frame_pixel_format_f16.Bitmap();
  ASSERT_EQ(2, bitmap.width());
  ASSERT_EQ(2, bitmap.height());
  ASSERT_TRUE(SkColorSpace::Equals(srgb_linear.get(), bitmap.colorSpace()));
}

TEST_F(ImageFrameTest, SetRGBAPremultiplyF16Buffer) {
  ImageFrame::PixelDataF16 premul_f16;
  ImageFrame::SetRGBAPremultiplyF16Buffer(&premul_f16, &src_f16, 1);

  float f32_from_src_f16[4];
  ConvertF16ToF32(f32_from_src_f16, src_f16);
  for (int i = 0; i < 3; i++)
    f32_from_src_f16[i] *= f32_from_src_f16[3];

  float f32_from_premul_f16[4];
  ConvertF16ToF32(f32_from_premul_f16, premul_f16);

  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(fabs(f32_from_src_f16[i] - f32_from_premul_f16[i]) <
                color_compoenent_tolerance);
  }
}

TEST_F(ImageFrameTest, SetPixelsOpaqueF16Buffer) {
  ImageFrame::PixelDataF16 opaque_f16;
  ImageFrame::SetPixelsOpaqueF16Buffer(&opaque_f16, &src_f16, 1);

  float f32_from_src_f16[4];
  ConvertF16ToF32(f32_from_src_f16, src_f16);

  float f32_from_opaque_f16[4];
  ConvertF16ToF32(f32_from_opaque_f16, opaque_f16);

  for (int i = 0; i < 3; i++)
    ASSERT_EQ(f32_from_src_f16[i], f32_from_opaque_f16[i]);
  ASSERT_EQ(1.0f, f32_from_opaque_f16[3]);
}

TEST_F(ImageFrameTest, BlendRGBARawF16Buffer) {
  ImageFrame::PixelData blended_8888(dst_8888);
  ImageFrame::BlendRGBARaw(&blended_8888, src_8888_r, src_8888_g, src_8888_b,
                           src_8888_a);

  ImageFrame::PixelDataF16 blended_f16 = dst_f16;
  ImageFrame::BlendRGBARawF16Buffer(&blended_f16, &src_f16, 1);

  float f32_from_blended_8888[4];
  ConvertN32ToF32(f32_from_blended_8888, blended_8888);

  float f32_from_blended_f16[4];
  ConvertF16ToF32(f32_from_blended_f16, blended_f16);

  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(fabs(f32_from_blended_8888[i] - f32_from_blended_f16[i]) <
                color_compoenent_tolerance);
  }
}

TEST_F(ImageFrameTest, BlendRGBAPremultipliedF16Buffer) {
  ImageFrame::PixelData blended_8888(dst_8888);
  ImageFrame::BlendRGBAPremultiplied(&blended_8888, src_8888_r, src_8888_g,
                                     src_8888_b, src_8888_a);

  ImageFrame::PixelDataF16 blended_f16 = dst_f16;
  ImageFrame::BlendRGBAPremultipliedF16Buffer(&blended_f16, &src_f16, 1);

  float f32_from_blended_8888[4];
  ConvertN32ToF32(f32_from_blended_8888, blended_8888);

  float f32_from_blended_f16[4];
  ConvertF16ToF32(f32_from_blended_f16, blended_f16);

  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(fabs(f32_from_blended_8888[i] - f32_from_blended_f16[i]) <
                color_compoenent_tolerance);
  }
}

}  // namespace
}  // namespace blink
