// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_session.h"

#include "base/fuchsia/fuchsia_logging.h"

namespace ui {

namespace {

// Max number of commands that will fit in a single message.
//
// TODO(sergeyu): Improve this logic when FIDL provides a mechanism to estimate
// message size, see https://fuchsia.atlassian.net/browse/FIDL-212 .
constexpr size_t kCommandsPerMessage =
    std::max(static_cast<size_t>(ZX_CHANNEL_MAX_MSG_HANDLES),
             (ZX_CHANNEL_MAX_MSG_BYTES - sizeof(fidl_message_header_t) -
              sizeof(fidl_vector_t)) /
                 sizeof(fuchsia::ui::gfx::Command));

// Helper function for all resource creation functions.
fuchsia::ui::gfx::Command NewCreateResourceCommand(
    ScenicSession::ResourceId resource_id,
    fuchsia::ui::gfx::ResourceArgs resource) {
  fuchsia::ui::gfx::CreateResourceCmd create_resource;
  create_resource.id = resource_id;
  create_resource.resource = std::move(resource);

  fuchsia::ui::gfx::Command command;
  command.set_create_resource(std::move(create_resource));

  return command;
}

fuchsia::ui::gfx::Command NewReleaseResourceCommand(
    ScenicSession::ResourceId resource_id) {
  fuchsia::ui::gfx::ReleaseResourceCmd release_resource;
  release_resource.id = resource_id;

  fuchsia::ui::gfx::Command command;
  command.set_release_resource(std::move(release_resource));

  return command;
}

}  // namespace

ScenicSession::ScenicSession(fuchsia::ui::scenic::Scenic* scenic,
                             ScenicSessionListener* listener)
    : listener_(listener), session_listener_binding_(this) {
  scenic->CreateSession(session_.NewRequest(),
                        session_listener_binding_.NewBinding());
  session_.set_error_handler([this]() {
    Close();
    listener_->OnScenicError("ScenicSession disconnected unexpectedly.");
  });
}

ScenicSession::~ScenicSession() {
  DCHECK_EQ(resource_count_, 0);
}

void ScenicSession::ReleaseResource(ResourceId resource_id) {
  resource_count_--;
  EnqueueGfxCommand(NewReleaseResourceCommand(resource_id));
}

ScenicSession::ResourceId ScenicSession::CreateMemory(
    base::SharedMemoryHandle vmo,
    fuchsia::images::MemoryType memory_type) {
  DCHECK(vmo.IsValid());

  fuchsia::ui::gfx::MemoryArgs memory;
  memory.vmo = zx::vmo(vmo.GetHandle());
  memory.memory_type = memory_type;

  fuchsia::ui::gfx::ResourceArgs resource;
  resource.set_memory(std::move(memory));

  ResourceId memory_id = AllocateResourceId();
  EnqueueGfxCommand(NewCreateResourceCommand(memory_id, std::move(resource)));

  return memory_id;
}

ScenicSession::ResourceId ScenicSession::CreateImage(
    ResourceId memory_id,
    ResourceId memory_offset,
    fuchsia::images::ImageInfo info) {
  fuchsia::ui::gfx::ImageArgs image;
  image.memory_id = memory_id;
  image.memory_offset = memory_offset;
  image.info = std::move(info);

  fuchsia::ui::gfx::ResourceArgs resource;
  resource.set_image(std::move(image));

  ResourceId image_id = AllocateResourceId();
  EnqueueGfxCommand(NewCreateResourceCommand(image_id, std::move(resource)));

  return image_id;
}

ScenicSession::ResourceId ScenicSession::ImportResource(
    fuchsia::ui::gfx::ImportSpec spec,
    zx::eventpair import_token) {
  DCHECK(import_token);

  ResourceId resource_id = AllocateResourceId();
  fuchsia::ui::gfx::ImportResourceCmd import_resource;
  import_resource.id = resource_id;
  import_resource.token = std::move(import_token);
  import_resource.spec = spec;

  fuchsia::ui::gfx::Command command;
  command.set_import_resource(std::move(import_resource));
  EnqueueGfxCommand(std::move(command));

  return resource_id;
}

ScenicSession::ResourceId ScenicSession::CreateEntityNode() {
  fuchsia::ui::gfx::ResourceArgs resource;
  resource.set_entity_node(fuchsia::ui::gfx::EntityNodeArgs());

  ResourceId node_id = AllocateResourceId();
  EnqueueGfxCommand(NewCreateResourceCommand(node_id, std::move(resource)));

  return node_id;
}

ScenicSession::ResourceId ScenicSession::CreateShapeNode() {
  fuchsia::ui::gfx::ResourceArgs resource;
  resource.set_shape_node(fuchsia::ui::gfx::ShapeNodeArgs());

  ResourceId node_id = AllocateResourceId();
  EnqueueGfxCommand(NewCreateResourceCommand(node_id, std::move(resource)));

  return node_id;
}

void ScenicSession::AddNodeChild(ResourceId node_id, ResourceId child_id) {
  fuchsia::ui::gfx::AddChildCmd add_child;
  add_child.node_id = node_id;
  add_child.child_id = child_id;

  fuchsia::ui::gfx::Command command;
  command.set_add_child(std::move(add_child));
  EnqueueGfxCommand(std::move(command));
}

void ScenicSession::SetNodeTranslation(ResourceId node_id,
                                       const float translation[3]) {
  fuchsia::ui::gfx::SetTranslationCmd set_translation;
  set_translation.id = node_id;
  set_translation.value.variable_id = 0;
  set_translation.value.value.x = translation[0];
  set_translation.value.value.y = translation[1];
  set_translation.value.value.z = translation[2];

  fuchsia::ui::gfx::Command command;
  command.set_set_translation(std::move(set_translation));
  EnqueueGfxCommand(std::move(command));
}

ScenicSession::ResourceId ScenicSession::CreateRectangle(float width,
                                                         float height) {
  fuchsia::ui::gfx::Value width_value;
  width_value.set_vector1(width);

  fuchsia::ui::gfx::Value height_value;
  height_value.set_vector1(height);

  fuchsia::ui::gfx::RectangleArgs rectangle;
  rectangle.width = std::move(width_value);
  rectangle.height = std::move(height_value);

  fuchsia::ui::gfx::ResourceArgs resource;
  resource.set_rectangle(std::move(rectangle));

  ResourceId rectangle_id = AllocateResourceId();
  EnqueueGfxCommand(
      NewCreateResourceCommand(rectangle_id, std::move(resource)));

  return rectangle_id;
}

ScenicSession::ResourceId ScenicSession::CreateMaterial() {
  fuchsia::ui::gfx::ResourceArgs resource;
  resource.set_material(fuchsia::ui::gfx::MaterialArgs());

  ResourceId material_id = AllocateResourceId();
  EnqueueGfxCommand(NewCreateResourceCommand(material_id, std::move(resource)));
  return material_id;
}

void ScenicSession::SetNodeMaterial(ResourceId node_id,
                                    ResourceId material_id) {
  fuchsia::ui::gfx::SetMaterialCmd set_material;
  set_material.node_id = node_id;
  set_material.material_id = material_id;

  fuchsia::ui::gfx::Command command;
  command.set_set_material(std::move(set_material));
  EnqueueGfxCommand(std::move(command));
}

void ScenicSession::SetNodeShape(ResourceId node_id, ResourceId shape_id) {
  fuchsia::ui::gfx::SetShapeCmd set_shape;
  set_shape.node_id = node_id;
  set_shape.shape_id = shape_id;

  fuchsia::ui::gfx::Command command;
  command.set_set_shape(std::move(set_shape));
  EnqueueGfxCommand(std::move(command));
}

void ScenicSession::SetMaterialTexture(ResourceId material_id,
                                       ResourceId texture_id) {
  fuchsia::ui::gfx::SetTextureCmd set_texture;
  set_texture.material_id = material_id;
  set_texture.texture_id = texture_id;

  fuchsia::ui::gfx::Command command;
  command.set_set_texture(std::move(set_texture));
  EnqueueGfxCommand(std::move(command));
}

void ScenicSession::SetEventMask(ResourceId resource_id, uint32_t event_mask) {
  fuchsia::ui::gfx::SetEventMaskCmd set_event_mask;
  set_event_mask.id = resource_id;
  set_event_mask.event_mask = event_mask;

  fuchsia::ui::gfx::Command command;
  command.set_set_event_mask(std::move(set_event_mask));
  EnqueueGfxCommand(std::move(command));
}

void ScenicSession::Present() {
  Flush();

  // Pass empty non-null vectors for acquire_fences and release_fences.
  fidl::VectorPtr<zx::event> acquire_fences(static_cast<size_t>(0));
  fidl::VectorPtr<zx::event> release_fences(static_cast<size_t>(0));
  session_->Present(0, std::move(acquire_fences), std::move(release_fences),
                    [](fuchsia::images::PresentationInfo info) {});
}

void ScenicSession::Close() {
  session_.set_error_handler(nullptr);
  session_.Unbind();
  session_listener_binding_.Unbind();
}

void ScenicSession::OnError(fidl::StringPtr error) {
  Close();
  listener_->OnScenicError(error);
}

void ScenicSession::OnEvent(
    fidl::VectorPtr<fuchsia::ui::scenic::Event> events) {
  listener_->OnScenicEvents(events.get());
}

ScenicSession::ResourceId ScenicSession::AllocateResourceId() {
  resource_count_++;
  return next_resource_id_++;
}

void ScenicSession::EnqueueGfxCommand(fuchsia::ui::gfx::Command command) {
  fuchsia::ui::scenic::Command scenic_command;
  scenic_command.set_gfx(std::move(command));
  queued_commands_.push_back(std::move(scenic_command));

  DCHECK_LE(queued_commands_->size(), kCommandsPerMessage);
  if (queued_commands_->size() == kCommandsPerMessage)
    Flush();
}

void ScenicSession::Flush() {
  if (!queued_commands_->empty())
    session_->Enqueue(std::move(queued_commands_));
}

}  // namespace ui
