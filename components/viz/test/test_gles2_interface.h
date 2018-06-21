// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_
#define COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_

#include <stddef.h>
#include <unordered_map>
#include <unordered_set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "components/viz/test/ordered_texture_map.h"
#include "components/viz/test/test_texture.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

class TestContextSupport;

class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  TestGLES2Interface();
  ~TestGLES2Interface() override;

  void GenTextures(GLsizei n, GLuint* textures) override;
  void GenBuffers(GLsizei n, GLuint* buffers) override;
  void GenFramebuffers(GLsizei n, GLuint* framebuffers) override;
  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override;
  void GenQueriesEXT(GLsizei n, GLuint* queries) override;

  void DeleteTextures(GLsizei n, const GLuint* textures) override;
  void DeleteBuffers(GLsizei n, const GLuint* buffers) override;
  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override;
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;

  GLuint CreateShader(GLenum type) override;
  GLuint CreateProgram() override;

  void BindTexture(GLenum target, GLuint texture) override;

  void GetIntegerv(GLenum pname, GLint* params) override;
  void GetShaderiv(GLuint shader, GLenum pname, GLint* params) override;
  void GetProgramiv(GLuint program, GLenum pname, GLint* params) override;
  void GetShaderPrecisionFormat(GLenum shadertype,
                                GLenum precisiontype,
                                GLint* range,
                                GLint* precision) override;
  GLenum CheckFramebufferStatus(GLenum target) override;

  void ActiveTexture(GLenum target) override;
  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) override;
  void UseProgram(GLuint program) override;
  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) override;
  void DrawElements(GLenum mode,
                    GLsizei count,
                    GLenum type,
                    const void* indices) override;
  void ClearColor(GLclampf red,
                  GLclampf green,
                  GLclampf blue,
                  GLclampf alpha) override;
  void ClearStencil(GLint s) override;
  void Clear(GLbitfield mask) override;
  void Flush() override;
  void Finish() override;
  void ShallowFinishCHROMIUM() override;
  void ShallowFlushCHROMIUM() override;
  void Enable(GLenum cap) override;
  void Disable(GLenum cap) override;

  void BindBuffer(GLenum target, GLuint buffer) override;
  void BindRenderbuffer(GLenum target, GLuint buffer) override;
  void BindFramebuffer(GLenum target, GLuint buffer) override;

  void PixelStorei(GLenum pname, GLint param) override;

  void TexImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) override;
  void TexSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override;
  void TexStorage2DEXT(GLenum target,
                       GLsizei levels,
                       GLenum internalformat,
                       GLsizei width,
                       GLsizei height) override;
  void TexStorage2DImageCHROMIUM(GLenum target,
                                 GLenum internalformat,
                                 GLenum bufferusage,
                                 GLsizei width,
                                 GLsizei height) override;
  void TexParameteri(GLenum target, GLenum pname, GLint param) override;
  void GetTexParameteriv(GLenum target, GLenum pname, GLint* value) override;

  void CompressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            GLsizei image_size,
                            const void* data) override;
  GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                             GLsizei width,
                             GLsizei height,
                             GLenum internalformat) override;
  void DestroyImageCHROMIUM(GLuint image_id) override;
  void BindTexImage2DCHROMIUM(GLenum target, GLint image_id) override;
  void ReleaseTexImage2DCHROMIUM(GLenum target, GLint image_id) override;
  void FramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer) override;
  void FramebufferTexture2D(GLenum target,
                            GLenum attachment,
                            GLenum textarget,
                            GLuint texture,
                            GLint level) override;
  void RenderbufferStorage(GLenum target,
                           GLenum internalformat,
                           GLsizei width,
                           GLsizei height) override;

  void* MapBufferCHROMIUM(GLuint target, GLenum access) override;
  GLboolean UnmapBufferCHROMIUM(GLuint target) override;
  void BufferData(GLenum target,
                  GLsizeiptr size,
                  const void* data,
                  GLenum usage) override;

  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

  void BeginQueryEXT(GLenum target, GLuint id) override;
  void EndQueryEXT(GLenum target) override;
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;

  void DiscardFramebufferEXT(GLenum target,
                             GLsizei count,
                             const GLenum* attachments) override;
  void ProduceTextureDirectCHROMIUM(GLuint texture, GLbyte* mailbox) override;
  GLuint CreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox) override;

  void ResizeCHROMIUM(GLuint width,
                      GLuint height,
                      float device_scale,
                      GLenum color_space,
                      GLboolean has_alpha) override;
  void LoseContextCHROMIUM(GLenum current, GLenum other) override;
  GLenum GetGraphicsResetStatusKHR() override;

  size_t NumTextures() const;
  GLuint TextureAt(int i) const;

  size_t NumUsedTextures() const { return used_textures_.size(); }
  bool UsedTexture(int texture) const {
    return base::ContainsKey(used_textures_, texture);
  }
  void ResetUsedTextures() { used_textures_.clear(); }

  size_t NumFramebuffers() const;
  size_t NumRenderbuffers() const;

  scoped_refptr<TestTexture> BoundTexture(GLenum target);
  scoped_refptr<TestTexture> UnboundTexture(GLuint texture);
  GLuint CreateExternalTexture();
  bool IsContextLost() { return context_lost_; }
  void set_test_support(TestContextSupport* test_support) {
    test_support_ = test_support;
  }
  const gpu::Capabilities& test_capabilities() const {
    return test_capabilities_;
  }
  const gpu::SyncToken& last_waited_sync_token() const {
    return last_waited_sync_token_;
  }
  void set_context_lost(bool context_lost) { context_lost_ = context_lost; }
  void set_times_bind_texture_succeeds(int times);

  void set_have_extension_io_surface(bool have);
  void set_have_extension_egl_image(bool have);
  void set_have_post_sub_buffer(bool have);
  void set_have_swap_buffers_with_bounds(bool have);
  void set_have_commit_overlay_planes(bool have);
  void set_have_discard_framebuffer(bool have);
  void set_support_compressed_texture_etc1(bool support);
  void set_support_texture_format_bgra8888(bool support);
  void set_support_texture_storage(bool support);
  void set_support_texture_usage(bool support);
  void set_support_sync_query(bool support);
  void set_support_texture_rectangle(bool support);
  void set_support_texture_half_float_linear(bool support);
  void set_support_texture_norm16(bool support);
  void set_msaa_is_slow(bool msaa_is_slow);
  void set_gpu_rasterization(bool gpu_rasterization);
  void set_avoid_stencil_buffers(bool avoid_stencil_buffers);
  void set_enable_dc_layers(bool support);
  void set_support_multisample_compatibility(bool support);
  void set_support_texture_storage_image(bool support);
  void set_support_texture_npot(bool support);
  void set_max_texture_size(int size);
  // When set, MapBufferCHROMIUM will return NULL after this many times.
  void set_times_map_buffer_chromium_succeeds(int times) {
    times_map_buffer_chromium_succeeds_ = times;
  }

  virtual GLuint NextTextureId();
  virtual void RetireTextureId(GLuint id);

  virtual GLuint NextBufferId();
  virtual void RetireBufferId(GLuint id);

  virtual GLuint NextImageId();
  virtual void RetireImageId(GLuint id);

  virtual GLuint NextFramebufferId();
  virtual void RetireFramebufferId(GLuint id);

  virtual GLuint NextRenderbufferId();
  virtual void RetireRenderbufferId(GLuint id);

  void SetMaxSamples(int max_samples);
  void set_context_lost_callback(base::OnceClosure callback) {
    context_lost_callback_ = std::move(callback);
  }

  int width() const { return width_; }
  int height() const { return height_; }
  bool reshape_called() const { return reshape_called_; }
  void clear_reshape_called() { reshape_called_ = false; }
  float scale_factor() const { return scale_factor_; }

  enum UpdateType { NO_UPDATE = 0, PREPARE_TEXTURE, POST_SUB_BUFFER };

  gfx::Rect update_rect() const { return update_rect_; }

  UpdateType last_update_type() { return last_update_type_; }

 protected:
  struct TextureTargets {
    TextureTargets();
    ~TextureTargets();

    void BindTexture(GLenum target, GLuint id);
    void UnbindTexture(GLuint id);

    GLuint BoundTexture(GLenum target);

   private:
    using TargetTextureMap = std::unordered_map<GLenum, GLuint>;
    TargetTextureMap bound_textures_;
  };

  struct Buffer {
    Buffer();
    ~Buffer();

    GLenum target;
    std::unique_ptr<uint8_t[]> pixels;
    size_t size;

   private:
    DISALLOW_COPY_AND_ASSIGN(Buffer);
  };

  struct Image {
    Image();
    ~Image();

    std::unique_ptr<uint8_t[]> pixels;

   private:
    DISALLOW_COPY_AND_ASSIGN(Image);
  };

  struct Namespace : public base::RefCountedThreadSafe<Namespace> {
    Namespace();

    // Protects all fields.
    base::Lock lock;
    unsigned next_buffer_id;
    unsigned next_image_id;
    unsigned next_texture_id;
    unsigned next_renderbuffer_id;
    std::unordered_map<unsigned, std::unique_ptr<Buffer>> buffers;
    std::unordered_set<unsigned> images;
    OrderedTextureMap textures;
    std::unordered_set<unsigned> renderbuffer_set;

   private:
    friend class base::RefCountedThreadSafe<Namespace>;
    ~Namespace();
    DISALLOW_COPY_AND_ASSIGN(Namespace);
  };

  void CheckTextureIsBound(GLenum target);

  void CreateNamespace();
  static const GLuint kExternalTextureId;
  GLuint BoundTextureId(GLenum target);
  unsigned context_id_;
  gpu::Capabilities test_capabilities_;
  int times_bind_texture_succeeds_;
  int times_end_query_succeeds_;
  bool context_lost_;
  int times_map_buffer_chromium_succeeds_;
  base::OnceClosure context_lost_callback_;
  std::unordered_set<unsigned> used_textures_;
  unsigned next_program_id_;
  std::unordered_set<unsigned> program_set_;
  unsigned next_shader_id_;
  std::unordered_set<unsigned> shader_set_;
  unsigned next_framebuffer_id_;
  std::unordered_set<unsigned> framebuffer_set_;
  unsigned current_framebuffer_;
  std::vector<TestGLES2Interface*> shared_contexts_;
  bool reshape_called_;
  int width_;
  int height_;
  float scale_factor_;
  TestContextSupport* test_support_;
  gfx::Rect update_rect_;
  UpdateType last_update_type_;
  GLuint64 next_insert_fence_sync_;
  gpu::SyncToken last_waited_sync_token_;
  int unpack_alignment_;

  base::flat_map<unsigned, unsigned> bound_buffer_;
  TextureTargets texture_targets_;

  scoped_refptr<Namespace> namespace_;
  static Namespace* shared_namespace_;
  base::WeakPtrFactory<TestGLES2Interface> weak_ptr_factory_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_
