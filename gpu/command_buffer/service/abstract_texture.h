// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_H_

#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/gpu_gles2_export.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef unsigned GLenum;
typedef int GLsizei;
typedef int GLint;
typedef unsigned int GLuint;

namespace gl {
class GLImage;
}  // namespace gl

namespace gpu {
namespace gles2 {
class GLStreamTextureImage;

// An AbstractTexture enables access to GL textures from the GPU process, for
// things that set up textures using some client's decoder.  Creating an
// AbstractTexture is similar to "glGenTexture", and deleting it is similar to
// calling "glDeleteTextures".
//
// There are some subtle differences.  Deleting an AbstractTexture doesn't
// guarantee that the underlying platform texture has been deleted if it's
// referenced elsewhere.  For example, if it has been sent via mailbox to some
// other context, then it might still be around after the AbstractTexture has
// been destroyed.
//
// Also, an AbstractTexture is tied to the decoder that created it, in the sense
// that destroying the decoder drops the reference to the texture just as if the
// AbstractTexture were destroyed.  While it's okay for the AbstractTexture to
// exist beyond decoder destruction, it won't actually refer to a texture after
// that.  This makes it easier for the holder to ignore stub destruction; the
// texture will be cleaned up properly, as needed.
class GPU_GLES2_EXPORT AbstractTexture {
 public:
  // The texture is guaranteed to be around while |this| exists, as long as
  // the decoder isn't destroyed / context isn't lost.
  virtual ~AbstractTexture() = default;

  // Return our TextureBase, useful mostly for creating a mailbox.  This may
  // return null if the texture has been destroyed.
  virtual TextureBase* GetTextureBase() const = 0;

  // Set a texture parameter.  The GL context must be current.
  virtual void SetParameteri(GLenum pname, GLint param) = 0;

  // Set |image| to be our stream texture image, using |service_id| in place
  // of our real service id when the client tries to bind us.  This must also
  // guarantee that CopyTexImage() is called before drawing, so that |image|
  // may update the stream texture.  This will do nothing if the texture has
  // been destroyed.
  virtual void BindStreamTextureImage(GLStreamTextureImage* image,
                                      GLuint service_id) = 0;

  // Attaches |image| to the AbstractTexture.  If |client_managed| is true, then
  // the decoder does not call GLImage::Copy/Bind.  Further, the decoder
  // guarantees that ScheduleOverlayPlane will be called if the texture is ever
  // promoted to an overlay.
  virtual void BindImage(gl::GLImage* image, bool client_managed) = 0;

  unsigned int service_id() const { return GetTextureBase()->service_id(); }
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_H_
