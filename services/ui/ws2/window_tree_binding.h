// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_TREE_BINDING_H_
#define SERVICES_UI_WS2_WINDOW_TREE_BINDING_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws2/window_tree_binding.h"

namespace aura {
class Window;
}

namespace ui {
namespace ws2 {

class WindowService;
class WindowTree;

// Owns the mojo structures and WindowTree for a single client.
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowTreeBinding {
 public:
  WindowTreeBinding();
  ~WindowTreeBinding();

  // See WindowTree for details on parameters and when to use.
  // |window_tree_client_ptr| may be null for tests.
  void InitForEmbed(WindowService* window_service,
                    mojom::WindowTreeClientPtr window_tree_client_ptr,
                    mojom::WindowTreeClient* window_tree_client,
                    aura::Window* initial_root,
                    base::OnceClosure connection_lost_callback);

  // See WindowTree for details on parameters and when to use.
  void InitFromFactory(WindowService* window_service,
                       mojom::WindowTreeRequest window_tree_request,
                       mojom::WindowTreeClientPtr window_tree_client,
                       base::OnceClosure connection_lost_callback);

  WindowTree* window_tree() { return window_tree_.get(); }

 private:
  friend class WindowTree;

  void CreateBinding(mojom::WindowTreeRequest window_tree_request,
                     base::OnceClosure connection_lost_callback);

  mojom::WindowTreeClientPtr window_tree_client_;
  std::unique_ptr<WindowTree> window_tree_;
  std::unique_ptr<mojo::Binding<mojom::WindowTree>> binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeBinding);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_TREE_BINDING_H_
