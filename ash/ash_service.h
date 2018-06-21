// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SERVICE_H_
#define ASH_ASH_SERVICE_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/ui/gpu_host/gpu_host_delegate.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace aura {
class Env;
}  // namespace aura

namespace base {
class Thread;
}

namespace chromeos {
namespace system {
class ScopedFakeStatisticsProvider;
}
}  // namespace chromeos

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}

namespace service_manager {
struct EmbeddedServiceInfo;
}

namespace ui {
class Gpu;
class InputDeviceController;
namespace gpu_host {
class GpuHost;
}
namespace ws2 {
class HostContextFactory;
}
}  // namespace ui

namespace views {
class ViewsDelegate;
}

namespace viz {
class HostFrameSinkManager;
}

namespace wm {
class WMState;
}
namespace ash {

class NetworkConnectDelegateMus;

// Used to export Ash's mojo services, specifically the interfaces defined in
// Ash's manifest.json. Also responsible for creating the
// UI-Service/WindowService when ash runs out of process.
class ASH_EXPORT AshService : public service_manager::Service,
                              public service_manager::mojom::ServiceFactory,
                              public ui::gpu_host::GpuHostDelegate {
 public:
  AshService();
  ~AshService() override;

  // Returns an appropriate EmbeddedServiceInfo that creates AshService.
  static service_manager::EmbeddedServiceInfo CreateEmbeddedServiceInfo();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle handle) override;

  // service_manager::mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest service,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override;

 private:
  // Does initialization necessary when ash runs out of process. This is called
  // once the service starts (from OnStart()).
  void InitForOop();

  void BindServiceFactory(
      service_manager::mojom::ServiceFactoryRequest request);

  void CreateFrameSinkManager();

  // ui::ws::GpuHostDelegate:
  void OnGpuServiceInitialized() override;

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  std::unique_ptr<::wm::WMState> wm_state_;

  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  std::unique_ptr<ui::gpu_host::GpuHost> gpu_host_;

  std::unique_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;

  // IO thread for GPU and discardable shared memory IPC.
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<ui::Gpu> gpu_;
  std::unique_ptr<ui::ws2::HostContextFactory> context_factory_;

  std::unique_ptr<aura::Env> env_;

  std::unique_ptr<views::ViewsDelegate> views_delegate_;

  std::unique_ptr<NetworkConnectDelegateMus> network_connect_delegate_;
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      statistics_provider_;

  std::unique_ptr<ui::InputDeviceController> input_device_controller_;

  // Whether this class initialized NetworkHandler and needs to clean it up.
  bool network_handler_initialized_ = false;

  // Whether this class initialized DBusThreadManager and needs to clean it up.
  bool dbus_thread_manager_initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(AshService);
};

}  // namespace ash

#endif  // ASH_ASH_SERVICE_H_
