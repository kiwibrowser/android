// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gles2_interface.h"

#include <memory>

#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace viz {
namespace {

static bool check_parameter_value(TestGLES2Interface* gl,
                                  GLenum pname,
                                  GLint expected_value) {
  GLint actual_value = 0;
  gl->GetTexParameteriv(GL_TEXTURE_2D, pname, &actual_value);
  return expected_value == actual_value;
}

static void expect_default_parameter_values(TestGLES2Interface* gl) {
  EXPECT_TRUE(check_parameter_value(gl, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  EXPECT_TRUE(check_parameter_value(gl, GL_TEXTURE_MIN_FILTER,
                                    GL_NEAREST_MIPMAP_LINEAR));
  EXPECT_TRUE(check_parameter_value(gl, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_TRUE(check_parameter_value(gl, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_TRUE(check_parameter_value(gl, GL_TEXTURE_USAGE_ANGLE, GL_NONE));
}

TEST(TestGLES2InterfaceTest, GetDefaultTextureParameterValues) {
  auto gl = std::make_unique<TestGLES2Interface>();

  GLuint texture;
  gl->GenTextures(1, &texture);
  gl->BindTexture(GL_TEXTURE_2D, texture);

  expect_default_parameter_values(gl.get());
}

TEST(TestGLES2InterfaceTest, SetAndGetTextureParameter) {
  auto gl = std::make_unique<TestGLES2Interface>();

  GLuint texture;
  gl->GenTextures(1, &texture);
  gl->BindTexture(GL_TEXTURE_2D, texture);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  EXPECT_TRUE(
      check_parameter_value(gl.get(), GL_TEXTURE_MIN_FILTER, GL_NEAREST));
}

TEST(TestGLES2InterfaceTest,
     SetAndGetMultipleTextureParametersOnMultipleTextures) {
  auto gl = std::make_unique<TestGLES2Interface>();

  // Set and get non-default texture parameters on the first texture.
  GLuint first_texture;
  gl->GenTextures(1, &first_texture);
  gl->BindTexture(GL_TEXTURE_2D, first_texture);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  EXPECT_TRUE(
      check_parameter_value(gl.get(), GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_TRUE(
      check_parameter_value(gl.get(), GL_TEXTURE_MAG_FILTER, GL_NEAREST));

  // Set and get different, non-default texture parameters on the second
  // texture.
  GLuint second_texture;
  gl->GenTextures(1, &second_texture);
  gl->BindTexture(GL_TEXTURE_2D, second_texture);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_NEAREST);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);

  EXPECT_TRUE(check_parameter_value(gl.get(), GL_TEXTURE_MIN_FILTER,
                                    GL_LINEAR_MIPMAP_NEAREST));
  EXPECT_TRUE(check_parameter_value(gl.get(), GL_TEXTURE_MAG_FILTER,
                                    GL_LINEAR_MIPMAP_LINEAR));

  // Get texture parameters on the first texture and verify they are still
  // intact.
  gl->BindTexture(GL_TEXTURE_2D, first_texture);

  EXPECT_TRUE(
      check_parameter_value(gl.get(), GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_TRUE(
      check_parameter_value(gl.get(), GL_TEXTURE_MAG_FILTER, GL_NEAREST));
}

TEST(TestGLES2InterfaceTest, UseMultipleRenderAndFramebuffers) {
  auto gl = std::make_unique<TestGLES2Interface>();

  GLuint ids[2];
  gl->GenFramebuffers(2, ids);
  EXPECT_NE(ids[0], ids[1]);
  gl->DeleteFramebuffers(2, ids);

  gl->GenRenderbuffers(2, ids);
  EXPECT_NE(ids[0], ids[1]);
  gl->DeleteRenderbuffers(2, ids);
}

}  // namespace
}  // namespace viz
