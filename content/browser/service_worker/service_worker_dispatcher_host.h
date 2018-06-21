// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"

namespace content {

class ServiceWorkerContextWrapper;

namespace service_worker_dispatcher_host_unittest {
class ServiceWorkerDispatcherHostTest;
class TestingServiceWorkerDispatcherHost;
FORWARD_DECLARE_TEST(ServiceWorkerDispatcherHostTest,
                     ProviderCreatedAndDestroyed);
FORWARD_DECLARE_TEST(ServiceWorkerDispatcherHostTest, CleanupOnRendererCrash);
}  // namespace service_worker_dispatcher_host_unittest

// ServiceWorkerDispatcherHost is a browser-side endpoint for the renderer to
// notify the browser a service worker provider is created.
// Unless otherwise noted, all methods are called on the IO thread.
//
// In order to keep ordering with navigation IPCs to avoid potential races,
// currently mojom::ServiceWorkerDispatcherHost interface is associated with the
// legacy IPC channel.
// TODO(leonhsl): Remove this class once we can understand how to move
// OnProviderCreated() to an isolated message pipe.
class CONTENT_EXPORT ServiceWorkerDispatcherHost
    : public mojom::ServiceWorkerDispatcherHost,
      public RenderProcessHostObserver {
 public:
  // Called on the UI thread.
  ServiceWorkerDispatcherHost(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int render_process_id);

  void AddBinding(mojom::ServiceWorkerDispatcherHostAssociatedRequest request);

  // Called on the UI thread.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

 protected:
  ~ServiceWorkerDispatcherHost() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<ServiceWorkerDispatcherHost>;
  friend class service_worker_dispatcher_host_unittest::
      ServiceWorkerDispatcherHostTest;
  friend class service_worker_dispatcher_host_unittest::
      TestingServiceWorkerDispatcherHost;
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_dispatcher_host_unittest::ServiceWorkerDispatcherHostTest,
      ProviderCreatedAndDestroyed);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_dispatcher_host_unittest::ServiceWorkerDispatcherHostTest,
      CleanupOnRendererCrash);

  // mojom::ServiceWorkerDispatcherHost implementation
  void OnProviderCreated(ServiceWorkerProviderHostInfo info) override;

  void RemoveAllProviderHostsForProcess();

  const int render_process_id_;
  // Only accessed on the IO thread.
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  mojo::AssociatedBindingSet<mojom::ServiceWorkerDispatcherHost> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
