// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/embedding.h"

#include <utility>

#include "base/bind.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_tree.h"
#include "services/ui/ws2/window_tree_binding.h"
#include "ui/aura/window.h"

namespace ui {
namespace ws2 {

Embedding::Embedding(WindowTree* embedding_tree,
                     aura::Window* window,
                     bool embedding_tree_intercepts_events)
    : embedding_tree_(embedding_tree),
      window_(window),
      embedding_tree_intercepts_events_(embedding_tree_intercepts_events) {
  DCHECK(embedding_tree_);
  DCHECK(window_);
}  // namespace ws2

Embedding::~Embedding() {
  if (!binding_)
    embedded_tree_->OnEmbeddingDestroyed(this);
}

void Embedding::Init(WindowService* window_service,
                     mojom::WindowTreeClientPtr window_tree_client_ptr,
                     mojom::WindowTreeClient* window_tree_client,
                     base::OnceClosure connection_lost_callback) {
  binding_ = std::make_unique<WindowTreeBinding>();
  binding_->InitForEmbed(window_service, std::move(window_tree_client_ptr),
                         window_tree_client, window_,
                         std::move(connection_lost_callback));
  embedded_tree_ = binding_->window_tree();
}

void Embedding::InitForEmbedInExistingTree(WindowTree* embedded_tree) {
  embedded_tree_ = embedded_tree;
}

}  // namespace ws2
}  // namespace ui
