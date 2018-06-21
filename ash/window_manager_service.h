
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WINDOW_MANAGER_SERVICE_H_
#define ASH_WINDOW_MANAGER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/ui/common/types.h"

namespace aura {
class WindowTreeClient;
}

namespace base {
class Thread;
}

namespace chromeos {
namespace system {
class ScopedFakeStatisticsProvider;
}
}  // namespace chromeos

namespace service_manager {
class Connector;
}

namespace views {
class AuraInit;
}

namespace ui {
class ImageCursorsSet;
}

namespace ash {
class AshTestHelper;
class NetworkConnectDelegateMus;
class WindowManager;

// Hosts the window manager and the ash system user interface for mash. This is
// also responsible for creating the UI Service. This is only used for --mash.
class ASH_EXPORT WindowManagerService
    : public service_manager::Service,
      public service_manager::mojom::ServiceFactory {
 public:
  // See WindowManager's constructor for details of
  // |show_primary_host_on_connect|.
  explicit WindowManagerService(bool show_primary_host_on_connect);
  ~WindowManagerService() override;

  WindowManager* window_manager() { return window_manager_.get(); }

  service_manager::Connector* GetConnector();

  void set_running_standalone(bool value) { running_standalone_ = value; }

 private:
  friend class ash::AshTestHelper;

  // If |init_network_handler| is true, chromeos::NetworkHandler is initialized.
  void InitWindowManager(
      std::unique_ptr<aura::WindowTreeClient> window_tree_client,
      bool init_network_handler);

  // Initializes lower-level OS-specific components (e.g. D-Bus services).
  void InitializeComponents(bool init_network_handler);
  void ShutdownComponents();

  void BindServiceFactory(
      service_manager::mojom::ServiceFactoryRequest request);

  void CreateUiServiceOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> resource_runner,
      service_manager::mojom::ServiceRequest service_request);
  void DestroyUiServiceOnBackgroundThread();

  // service_manager::mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest service_request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  const bool show_primary_host_on_connect_;
  bool running_standalone_ = false;

  std::unique_ptr<views::AuraInit> aura_init_;

  std::unique_ptr<WindowManager> window_manager_;

  std::unique_ptr<NetworkConnectDelegateMus> network_connect_delegate_;
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_;

  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  service_manager::BinderRegistry registry_;

  // Whether this class initialized NetworkHandler and needs to clean it up.
  bool network_handler_initialized_ = false;

  // Whether this class initialized DBusThreadManager and needs to clean it up.
  bool dbus_thread_manager_initialized_ = false;

  // Thread the UI Service runs on.
  std::unique_ptr<base::Thread> ui_thread_;

  // The ServiceContext created for the UI service. This is created (and
  // shutdown) on |ui_thread_|.
  std::unique_ptr<service_manager::ServiceContext> ui_service_context_;

  std::unique_ptr<ui::ImageCursorsSet> image_cursors_set_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerService);
};

}  // namespace ash

#endif  // ASH_WINDOW_MANAGER_SERVICE_H_
