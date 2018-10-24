// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_tree_binding.h"

#include <utility>

#include "base/bind.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_tree.h"
#include "ui/aura/window.h"

namespace ui {
namespace ws2 {

WindowTreeBinding::WindowTreeBinding() = default;

WindowTreeBinding::~WindowTreeBinding() = default;

void WindowTreeBinding::InitForEmbed(
    WindowService* window_service,
    mojom::WindowTreeClientPtr window_tree_client_ptr,
    mojom::WindowTreeClient* window_tree_client,
    aura::Window* initial_root,
    base::OnceClosure connection_lost_callback) {
  window_tree_client_ = std::move(window_tree_client_ptr);
  window_tree_ = window_service->CreateWindowTree(window_tree_client);
  mojom::WindowTreePtr window_tree_ptr;
  if (window_tree_client_) {
    auto window_tree_request = mojo::MakeRequest(&window_tree_ptr);
    CreateBinding(std::move(window_tree_request),
                  std::move(connection_lost_callback));
  }
  window_tree_->InitForEmbed(initial_root, std::move(window_tree_ptr));
}

void WindowTreeBinding::InitFromFactory(
    WindowService* window_service,
    mojom::WindowTreeRequest window_tree_request,
    mojom::WindowTreeClientPtr window_tree_client,
    base::OnceClosure connection_lost_callback) {
  window_tree_client_ = std::move(window_tree_client);
  window_tree_ = window_service->CreateWindowTree(window_tree_client_.get());
  CreateBinding(std::move(window_tree_request),
                std::move(connection_lost_callback));
  window_tree_->InitFromFactory();
}

void WindowTreeBinding::CreateBinding(
    mojom::WindowTreeRequest window_tree_request,
    base::OnceClosure connection_lost_callback) {
  binding_ = std::make_unique<mojo::Binding<mojom::WindowTree>>(
      window_tree_.get(), std::move(window_tree_request));
  binding_->set_connection_error_handler(std::move(connection_lost_callback));
}

}  // namespace ws2
}  // namespace ui
