// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"

namespace content {

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    int render_process_id)
    : render_process_id_(render_process_id), context_wrapper_(context_wrapper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ServiceWorkerDispatcherHost::~ServiceWorkerDispatcherHost() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ServiceWorkerDispatcherHost::AddBinding(
    mojom::ServiceWorkerDispatcherHostAssociatedRequest request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bindings_.AddBinding(this, std::move(request));
}

void ServiceWorkerDispatcherHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(crbug.com/736203) Try to remove this call. It should be unnecessary
  // because provider hosts remove themselves when their Mojo connection to the
  // renderer is destroyed. But if we don't remove the hosts immediately here,
  // collisions of <process_id, provider_id> can occur if |this| is reused for
  // another new renderer process due to reuse of the RenderProcessHost.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &ServiceWorkerDispatcherHost::RemoveAllProviderHostsForProcess,
          base::Unretained(this)));
}

void ServiceWorkerDispatcherHost::OnProviderCreated(
    ServiceWorkerProviderHostInfo info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnProviderCreated");
  ServiceWorkerContextCore* context = context_wrapper_->context();
  if (!context)
    return;
  if (context->GetProviderHost(render_process_id_, info.provider_id)) {
    bindings_.ReportBadMessage("SWDH_PROVIDER_CREATED_DUPLICATE_ID");
    return;
  }

  // Provider hosts for navigations are precreated on the browser process with a
  // browser-assigned id. The renderer process calls OnProviderCreated once it
  // creates the provider.
  if (ServiceWorkerUtils::IsBrowserAssignedProviderId(info.provider_id)) {
    if (info.type != blink::mojom::ServiceWorkerProviderType::kForWindow) {
      bindings_.ReportBadMessage(
          "SWDH_PROVIDER_CREATED_ILLEGAL_TYPE_NOT_WINDOW");
      return;
    }

    // Retrieve the provider host pre-created for the navigation.
    std::unique_ptr<ServiceWorkerProviderHost> provider_host =
        context->ReleaseProviderHost(ChildProcessHost::kInvalidUniqueID,
                                     info.provider_id);
    // If no host is found, create one.
    // TODO(crbug.com/789111#c14): This is probably not right, see bug.
    if (!provider_host) {
      context->AddProviderHost(ServiceWorkerProviderHost::Create(
          render_process_id_, std::move(info), context->AsWeakPtr()));
      return;
    }

    // Otherwise, complete initialization of the pre-created host.
    provider_host->CompleteNavigationInitialized(render_process_id_,
                                                 std::move(info));
    context->AddProviderHost(std::move(provider_host));
    return;
  }

  // Provider hosts for service workers don't call OnProviderCreated. They are
  // precreated and ServiceWorkerProviderHost::CompleteStartWorkerPreparation is
  // called during the startup sequence once a process is allocated.
  if (info.type == blink::mojom::ServiceWorkerProviderType::kForServiceWorker) {
    bindings_.ReportBadMessage(
        "SWDH_PROVIDER_CREATED_ILLEGAL_TYPE_SERVICE_WORKER");
    return;
  }

  context->AddProviderHost(ServiceWorkerProviderHost::Create(
      render_process_id_, std::move(info), context->AsWeakPtr()));
}

void ServiceWorkerDispatcherHost::RemoveAllProviderHostsForProcess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (context_wrapper_->context()) {
    context_wrapper_->context()->RemoveAllProviderHostsForProcess(
        render_process_id_);
  }
}

}  // namespace content
