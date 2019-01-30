// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_FACTORY_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_FACTORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_implementation.h"

namespace media {

class PictureBuffer;
class VaapiWrapper;

// Factory of platform dependent VaapiPictures.
class MEDIA_GPU_EXPORT VaapiPictureFactory {
 public:
  enum VaapiImplementation {
    kVaapiImplementationNone = 0,
    kVaapiImplementationDrm,
    kVaapiImplementationX11
  };

  VaapiPictureFactory();
  virtual ~VaapiPictureFactory();

  // Creates a VaapiPicture of picture_buffer.size() associated with
  // picture_buffer.id().
  virtual std::unique_ptr<VaapiPicture> Create(
      const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const PictureBuffer& picture_buffer);

  // Return the type of the VaapiPicture implementation for the given GL
  // implementation.
  VaapiImplementation GetVaapiImplementation(gl::GLImplementation gl_impl);

  // Gets the texture target used to bind EGLImages (either GL_TEXTURE_2D on X11
  // or GL_TEXTURE_EXTERNAL_OES on DRM).
  uint32_t GetGLTextureTarget();

  // Buffer format to use for output buffers backing PictureBuffers. This is
  // the format decoded frames in VASurfaces are converted into.
  gfx::BufferFormat GetBufferFormat();

 private:
  DISALLOW_COPY_AND_ASSIGN(VaapiPictureFactory);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_FACTORY_H_
