// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/image_reader_gl_owner.h"

#include <stdint.h>
#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace media {

class ImageReaderGLOwnerTest : public testing::Test {
 public:
  ImageReaderGLOwnerTest() {}
  ~ImageReaderGLOwnerTest() override {}

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(media::kAImageReaderVideoOutput);
    gl::init::InitializeGLOneOffImplementation(gl::kGLImplementationEGLGLES2,
                                               false, false, false, true);
    surface_ = new gl::PbufferGLSurfaceEGL(gfx::Size(320, 240));
    surface_->Initialize();

    share_group_ = new gl::GLShareGroup();
    context_ = new gl::GLContextEGL(share_group_.get());
    context_->Initialize(surface_.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context_->MakeCurrent(surface_.get()));

    image_reader_ = ImageReaderGLOwner::Create();
  }

  void TearDown() override {
    image_reader_ = nullptr;
    context_ = nullptr;
    share_group_ = nullptr;
    surface_ = nullptr;
    gl::init::ShutdownGL(false);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<TextureOwner> image_reader_;
  GLuint texture_id_ = 0;

  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLSurface> surface_;
  base::MessageLoop message_loop_;
};

TEST_F(ImageReaderGLOwnerTest, ImageReaderObjectCreation) {
  ASSERT_TRUE(image_reader_);
}

TEST_F(ImageReaderGLOwnerTest, ScopedJavaSurfaceCreation) {
  gl::ScopedJavaSurface temp = image_reader_->CreateJavaSurface();
  ASSERT_TRUE(temp.IsValid());
}

// Verify that ImageReaderGLOwner creates a bindable GL texture, and deletes
// it during destruction.
TEST_F(ImageReaderGLOwnerTest, GLTextureIsCreatedAndDestroyed) {
  // |texture_id| should not work anymore after we delete image_reader_.
  image_reader_ = nullptr;
  ASSERT_FALSE(glIsTexture(texture_id_));
}

// Make sure that image_reader_ remembers the correct context and surface.
TEST_F(ImageReaderGLOwnerTest, ContextAndSurfaceAreCaptured) {
  ASSERT_EQ(context_, image_reader_->GetContext());
  ASSERT_EQ(surface_, image_reader_->GetSurface());
}

// Verify that destruction works even if some other context is current.
TEST_F(ImageReaderGLOwnerTest, DestructionWorksWithWrongContext) {
  scoped_refptr<gl::GLSurface> new_surface(
      new gl::PbufferGLSurfaceEGL(gfx::Size(320, 240)));
  new_surface->Initialize();

  scoped_refptr<gl::GLShareGroup> new_share_group(new gl::GLShareGroup());
  scoped_refptr<gl::GLContext> new_context(
      new gl::GLContextEGL(new_share_group.get()));
  new_context->Initialize(new_surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(new_context->MakeCurrent(new_surface.get()));

  image_reader_ = nullptr;
  ASSERT_FALSE(glIsTexture(texture_id_));

  // |new_context| should still be current.
  ASSERT_TRUE(new_context->IsCurrent(new_surface.get()));

  new_context = nullptr;
  new_share_group = nullptr;
  new_surface = nullptr;
}

}  // namespace media
