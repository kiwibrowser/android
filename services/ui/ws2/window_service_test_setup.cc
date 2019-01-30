// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service_test_setup.h"

#include "services/ui/ws2/embedding.h"
#include "services/ui/ws2/gpu_interface_provider.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_tree.h"
#include "services/ui/ws2/window_tree_binding.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"

namespace ui {
namespace ws2 {
namespace {

class TestFocusRules : public wm::BaseFocusRules {
 public:
  TestFocusRules() = default;
  ~TestFocusRules() override = default;

  // wm::BaseFocusRules:
  bool SupportsChildActivation(aura::Window* window) const override {
    return window == window->GetRootWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFocusRules);
};

}  // namespace

WindowServiceTestSetup::WindowServiceTestSetup()
    // FocusController takes ownership of TestFocusRules.
    : focus_controller_(new TestFocusRules()) {
  if (gl::GetGLImplementation() == gl::kGLImplementationNone)
    gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  const bool enable_pixel_output = false;
  ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                       &context_factory_private);
  aura_test_helper_.SetUp(context_factory, context_factory_private);
  scoped_capture_client_ = std::make_unique<wm::ScopedCaptureClient>(
      aura_test_helper_.root_window());
  service_ =
      std::make_unique<WindowService>(&delegate_, nullptr, focus_controller());
  aura::client::SetFocusClient(root(), focus_controller());
  delegate_.set_top_level_parent(aura_test_helper_.root_window());

  window_tree_ = service_->CreateWindowTree(&window_tree_client_);
  window_tree_->InitFromFactory();
  window_tree_test_helper_ =
      std::make_unique<WindowTreeTestHelper>(window_tree_.get());
}

WindowServiceTestSetup::~WindowServiceTestSetup() {
  window_tree_test_helper_.reset();
  window_tree_.reset();
  service_.reset();
  scoped_capture_client_.reset();
  aura::client::SetFocusClient(root(), nullptr);
  aura_test_helper_.TearDown();
  ui::TerminateContextFactoryForTests();
}

std::unique_ptr<EmbeddingHelper> WindowServiceTestSetup::CreateEmbedding(
    aura::Window* embed_root,
    uint32_t flags) {
  auto embedding_helper = std::make_unique<EmbeddingHelper>();
  embedding_helper->embedding = window_tree_test_helper_->Embed(
      embed_root, nullptr, &embedding_helper->window_tree_client, flags);
  if (!embedding_helper->embedding)
    return nullptr;
  embedding_helper->window_tree = embedding_helper->embedding->embedded_tree();
  embedding_helper->window_tree_test_helper =
      std::make_unique<WindowTreeTestHelper>(embedding_helper->window_tree);
  embedding_helper->parent_window_tree =
      embedding_helper->embedding->embedding_tree();
  return embedding_helper;
}

EmbeddingHelper::EmbeddingHelper() = default;

EmbeddingHelper::~EmbeddingHelper() {
  if (!embedding)
    return;

  WindowTreeTestHelper(parent_window_tree).DestroyEmbedding(embedding);
}

}  // namespace ws2
}  // namespace ui
