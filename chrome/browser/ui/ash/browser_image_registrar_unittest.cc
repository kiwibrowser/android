// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/browser_image_registrar.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

using BrowserImageRegistrarTest = testing::Test;

TEST_F(BrowserImageRegistrarTest, Basic) {
  auto GetRegistrations =
      &BrowserImageRegistrar::GetActiveRegistrationsForTesting;

  EXPECT_EQ(0U, GetRegistrations().size());

  // Register an image.
  gfx::ImageSkia image1 = gfx::test::CreateImageSkia(15, 20);
  scoped_refptr<ImageRegistration> registration1 =
      BrowserImageRegistrar::RegisterImage(image1);
  EXPECT_EQ(1U, GetRegistrations().size());

  // Register a different image.
  gfx::ImageSkia image2 = gfx::test::CreateImageSkia(15, 20);
  EXPECT_NE(image2.GetBackingObject(), image1.GetBackingObject());
  scoped_refptr<ImageRegistration> registration2 =
      BrowserImageRegistrar::RegisterImage(image2);
  EXPECT_EQ(2U, GetRegistrations().size());
  EXPECT_NE(registration2->token(), registration1->token());

  // Register an image that's a copy of one which is already registered.
  gfx::ImageSkia image3 = image1;
  EXPECT_EQ(image3.GetBackingObject(), image1.GetBackingObject());
  scoped_refptr<ImageRegistration> registration3 =
      BrowserImageRegistrar::RegisterImage(image3);
  EXPECT_EQ(2U, GetRegistrations().size());
  EXPECT_EQ(registration3->token(), registration1->token());

  // Get rid of one reference to the twice-registered image and verify the
  // registration is still there.
  registration1 = nullptr;
  EXPECT_EQ(2U, GetRegistrations().size());

  registration3 = nullptr;
  EXPECT_EQ(1U, GetRegistrations().size());

  // Verify that resetting an image that was registered, thus dropping the
  // backing store's refcount, before releasing the registration ref, won't
  // cause a crash or any change in the registration list.
  image2 = gfx::ImageSkia();
  EXPECT_EQ(1U, GetRegistrations().size());

  registration2 = nullptr;
  EXPECT_EQ(0U, GetRegistrations().size());

  BrowserImageRegistrar::Shutdown();
}

}  // namespace
