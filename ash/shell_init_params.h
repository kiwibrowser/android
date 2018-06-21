// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include <memory>

#include "ash/ash_export.h"

namespace base {
class Value;
}

namespace service_manager {
class Connector;
}

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
namespace ws2 {
class GpuInterfaceProvider;
}
}

namespace ash {

class ShellDelegate;
class ShellPort;

struct ASH_EXPORT ShellInitParams {
  ShellInitParams();
  ShellInitParams(ShellInitParams&& other);
  ~ShellInitParams();

  std::unique_ptr<ShellPort> shell_port;
  std::unique_ptr<ShellDelegate> delegate;
  ui::ContextFactory* context_factory = nullptr;                 // Non-owning.
  ui::ContextFactoryPrivate* context_factory_private = nullptr;  // Non-owning.
  // Dictionary of pref values used by DisplayPrefs before
  // ShellObserver::OnLocalStatePrefServiceInitialized is called.
  std::unique_ptr<base::Value> initial_display_prefs;

  // Allows gpu interfaces to be injected while avoiding direct content
  // dependencies.
  std::unique_ptr<ui::ws2::GpuInterfaceProvider> gpu_interface_provider;

  // Connector used by Shell to establish connections.
  service_manager::Connector* connector = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
