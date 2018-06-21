// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/content_gpu_interface_provider.h"

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_client.h"
#include "content/public/browser/gpu_service_registry.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace ash {

// InterfaceBinderImpl handles the actual binding. The binding (and destruction
// of this object) has to happen on the io-thread.
class ContentGpuInterfaceProvider::InterfaceBinderImpl
    : public base::RefCountedThreadSafe<
          InterfaceBinderImpl,
          content::BrowserThread::DeleteOnIOThread> {
 public:
  InterfaceBinderImpl() = default;

  void BindGpuRequestOnGpuTaskRunner(ui::mojom::GpuRequest request) {
    auto gpu_client = content::GpuClient::Create(
        std::move(request),
        base::BindOnce(&InterfaceBinderImpl::OnGpuClientConnectionError, this));
    gpu_clients_.push_back(std::move(gpu_client));
  }

  void BindDiscardableSharedMemoryManagerOnGpuTaskRunner(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest
          request) {
    content::BindInterfaceInGpuProcess(std::move(request));
  }

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<InterfaceBinderImpl>;

  ~InterfaceBinderImpl() = default;

  void OnGpuClientConnectionError(content::GpuClient* client) {
    base::EraseIf(
        gpu_clients_,
        base::UniquePtrMatcher<content::GpuClient,
                               content::BrowserThread::DeleteOnIOThread>(
            client));
  }

  std::vector<std::unique_ptr<content::GpuClient,
                              content::BrowserThread::DeleteOnIOThread>>
      gpu_clients_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceBinderImpl);
};

ContentGpuInterfaceProvider::ContentGpuInterfaceProvider()
    : interface_binder_impl_(base::MakeRefCounted<InterfaceBinderImpl>()) {}

ContentGpuInterfaceProvider::~ContentGpuInterfaceProvider() = default;

void ContentGpuInterfaceProvider::RegisterGpuInterfaces(
    service_manager::BinderRegistry* registry) {
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner =
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO);
  registry->AddInterface(
      base::BindRepeating(&InterfaceBinderImpl::
                              BindDiscardableSharedMemoryManagerOnGpuTaskRunner,
                          interface_binder_impl_),
      gpu_task_runner);
  registry->AddInterface(
      base::BindRepeating(&InterfaceBinderImpl::BindGpuRequestOnGpuTaskRunner,
                          interface_binder_impl_),
      gpu_task_runner);
}

}  // namespace ash
