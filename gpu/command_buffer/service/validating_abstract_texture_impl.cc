// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/validating_abstract_texture_impl.h"

#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace gles2 {

ValidatingAbstractTextureImpl::ValidatingAbstractTextureImpl(
    scoped_refptr<TextureRef> texture_ref,
    DecoderContext* decoder_context,
    DestructionCB destruction_cb)
    : texture_ref_(std::move(texture_ref)),
      decoder_context_(decoder_context),
      destruction_cb_(std::move(destruction_cb)) {}

ValidatingAbstractTextureImpl::~ValidatingAbstractTextureImpl() {
  if (destruction_cb_)
    std::move(destruction_cb_).Run(this, std::move(texture_ref_));

  DCHECK(!texture_ref_);
}

TextureBase* ValidatingAbstractTextureImpl::GetTextureBase() const {
  if (!texture_ref_)
    return nullptr;
  return texture_ref_->texture();
}

void ValidatingAbstractTextureImpl::SetParameteri(GLenum pname, GLint param) {
  if (!texture_ref_)
    return;

  gl::ScopedTextureBinder binder(texture_ref_->texture()->target(),
                                 service_id());
  GetTextureManager()->SetParameteri(__func__, GetErrorState(),
                                     texture_ref_.get(), pname, param);
}

void ValidatingAbstractTextureImpl::BindImage(gl::GLImage* image,
                                              bool client_managed) {
  if (!texture_ref_)
    return;

  const GLint level = 0;

  Texture::ImageState state = Texture::ImageState::UNBOUND;
  if (client_managed && image)
    state = Texture::ImageState::BOUND;

  GetTextureManager()->SetLevelImage(texture_ref_.get(),
                                     texture_ref_->texture()->target(), level,
                                     image, state);
  GetTextureManager()->SetLevelCleared(
      texture_ref_.get(), texture_ref_->texture()->target(), level, true);
}

void ValidatingAbstractTextureImpl::BindStreamTextureImage(
    GLStreamTextureImage* image,
    GLuint service_id) {
  if (!texture_ref_)
    return;

  const GLint level = 0;

  // We set the state to UNBOUND, so that CopyTexImage is called.
  GetTextureManager()->SetLevelStreamTextureImage(
      texture_ref_.get(), texture_ref_->texture()->target(), level, image,
      Texture::ImageState::UNBOUND, service_id);
  GetTextureManager()->SetLevelCleared(
      texture_ref_.get(), texture_ref_->texture()->target(), level, true);
}

TextureManager* ValidatingAbstractTextureImpl::GetTextureManager() const {
  DCHECK(decoder_context_);
  return GetContextGroup()->texture_manager();
}

ContextGroup* ValidatingAbstractTextureImpl::GetContextGroup() const {
  DCHECK(decoder_context_);
  return decoder_context_->GetContextGroup();
}

ErrorState* ValidatingAbstractTextureImpl::GetErrorState() const {
  DCHECK(decoder_context_);
  return decoder_context_->GetErrorState();
}

void ValidatingAbstractTextureImpl::OnDecoderWillDestroy(bool have_context) {
  // If we don't have a context, then notify the TextureRef not to delete itself
  // if this is the last reference.
  destruction_cb_ = DestructionCB();
  decoder_context_ = nullptr;

  // If we already got rid of the texture ref, then there's nothing to do.
  if (!texture_ref_)
    return;

  // If we have no context, then notify the TextureRef in case it's the last
  // ref to the texture.
  if (!have_context)
    texture_ref_->ForceContextLost();
  texture_ref_ = nullptr;
}

TextureRef* ValidatingAbstractTextureImpl::GetTextureRefForTesting() {
  return texture_ref_.get();
}

}  // namespace gles2
}  // namespace gpu
