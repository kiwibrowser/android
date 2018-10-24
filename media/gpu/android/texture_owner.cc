// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_owner.h"

#include "base/threading/thread_task_runner_handle.h"
#include "media/gpu/android/android_image_reader_compat.h"
#include "media/gpu/android/image_reader_gl_owner.h"
#include "media/gpu/android/surface_texture_gl_owner.h"
#include "ui/gl/scoped_binders.h"

namespace media {

TextureOwner::TextureOwner()
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::ThreadTaskRunnerHandle::Get()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

TextureOwner::~TextureOwner() = default;

scoped_refptr<TextureOwner> TextureOwner::Create() {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  if (!texture_id)
    return nullptr;

  // Set the parameters on the texture.
  gl::ScopedActiveTexture active_texture(GL_TEXTURE0);
  gl::ScopedTextureBinder texture_binder(GL_TEXTURE_EXTERNAL_OES, texture_id);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  if (AndroidImageReader::GetInstance().IsSupported()) {
    // If image reader is supported, use it.
    return new ImageReaderGLOwner(texture_id);
  }

  // If not, fall back to legacy path.
  return new SurfaceTextureGLOwner(texture_id);
}

}  // namespace media
