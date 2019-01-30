// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker_task_queue.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/service_worker_task_queue_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/background_info.h"

using content::BrowserContext;

namespace extensions {

namespace {

void RunTask(LazyContextTaskQueue::PendingTask task,
             const ExtensionId& extension_id,
             int process_id,
             int thread_id) {
  auto params = std::make_unique<LazyContextTaskQueue::ContextInfo>(
      extension_id, content::RenderProcessHost::FromID(process_id), thread_id,
      GURL());
  std::move(task).Run(std::move(params));
}

void DidStartWorkerForPattern(LazyContextTaskQueue::PendingTask task,
                              const ExtensionId& extension_id,
                              int process_id,
                              int thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(RunTask, std::move(task), extension_id, process_id,
                     thread_id));
}

void DidStartWorkerFail() {
  DCHECK(false) << "DidStartWorkerFail";
  // TODO(lazyboy): Handle failure case.
}

void GetServiceWorkerInfoOnIO(
    const GURL& pattern,
    const ExtensionId& extension_id,
    content::ServiceWorkerContext* service_worker_context,
    LazyContextTaskQueue::PendingTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  service_worker_context->StartWorkerForPattern(
      pattern,
      base::BindOnce(&DidStartWorkerForPattern, std::move(task), extension_id),
      base::BindOnce(&DidStartWorkerFail));
}

}  // namespace

struct ServiceWorkerTaskQueue::TaskInfo {
 public:
  TaskInfo(const GURL& scope, PendingTask task)
      : service_worker_scope(scope), task(std::move(task)) {}
  TaskInfo(TaskInfo&& other) = default;
  TaskInfo& operator=(TaskInfo&& other) = default;

  GURL service_worker_scope;
  PendingTask task;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskInfo);
};

ServiceWorkerTaskQueue::ServiceWorkerTaskQueue(BrowserContext* browser_context)
    : browser_context_(browser_context), weak_factory_(this) {}

ServiceWorkerTaskQueue::~ServiceWorkerTaskQueue() {}

// static
ServiceWorkerTaskQueue* ServiceWorkerTaskQueue::Get(BrowserContext* context) {
  return ServiceWorkerTaskQueueFactory::GetForBrowserContext(context);
}

bool ServiceWorkerTaskQueue::ShouldEnqueueTask(BrowserContext* context,
                                               const Extension* extension) {
  // We call StartWorker every time we want to dispatch an event to an extension
  // Service worker.
  // TODO(lazyboy): Is that a problem?
  return true;
}

void ServiceWorkerTaskQueue::AddPendingTaskToDispatchEvent(
    LazyContextId* context_id,
    LazyContextTaskQueue::PendingTask task) {
  DCHECK(context_id->is_for_service_worker());

  // TODO(lazyboy): Do we need to handle incognito context?

  const ExtensionId& extension_id = context_id->extension_id();
  if (pending_registrations_.count(extension_id) > 0u) {
    // If the worker hasn't finished registration, wait for it to complete.
    pending_tasks_[extension_id].emplace_back(
        context_id->service_worker_scope(), std::move(task));
    return;
  }

  RunTaskAfterStartWorker(context_id, std::move(task));
}

void ServiceWorkerTaskQueue::ActivateExtension(const Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(lazyboy): Should we only register Service Worker during installation
  // and remember its success/failure state in prefs?
  const ExtensionId extension_id = extension->id();
  pending_registrations_.insert(extension_id);
  GURL script_url = extension->GetResourceURL(
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
  blink::mojom::ServiceWorkerRegistrationOptions option;
  option.scope = extension->url();
  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->RegisterServiceWorker(
          script_url, option,
          base::BindOnce(&ServiceWorkerTaskQueue::DidRegisterServiceWorker,
                         weak_factory_.GetWeakPtr(), extension_id));
}

void ServiceWorkerTaskQueue::DeactivateExtension(const Extension* extension) {
  GURL script_url = extension->GetResourceURL(
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
  const ExtensionId extension_id = extension->id();
  content::BrowserContext::GetStoragePartitionForSite(browser_context_,
                                                      extension->url())
      ->GetServiceWorkerContext()
      ->UnregisterServiceWorker(
          script_url,
          base::BindOnce(&ServiceWorkerTaskQueue::DidUnregisterServiceWorker,
                         weak_factory_.GetWeakPtr(), extension_id));
}

void ServiceWorkerTaskQueue::RunTaskAfterStartWorker(
    LazyContextId* context_id,
    LazyContextTaskQueue::PendingTask task) {
  DCHECK(context_id->is_for_service_worker());

  if (context_id->browser_context() != browser_context_)
    return;

  content::StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(
          context_id->browser_context(), context_id->service_worker_scope());
  content::ServiceWorkerContext* service_worker_context =
      partition->GetServiceWorkerContext();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &GetServiceWorkerInfoOnIO, context_id->service_worker_scope(),
          context_id->extension_id(), service_worker_context, std::move(task)));
}

void ServiceWorkerTaskQueue::DidRegisterServiceWorker(
    const ExtensionId& extension_id,
    bool success) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  DCHECK(registry);
  if (!registry->enabled_extensions().Contains(extension_id))
    return;

  std::vector<TaskInfo> pending_tasks;
  std::swap(pending_tasks_[extension_id], pending_tasks);

  if (!success)  // TODO(lazyboy): Handle.
    return;

  for (TaskInfo& task_info : pending_tasks) {
    LazyContextId context_id(browser_context_, extension_id,
                             task_info.service_worker_scope);
    // TODO(lazyboy): Minimize number of GetServiceWorkerInfoOnIO calls. We
    // only need to call it for each unique service_worker_scope.
    RunTaskAfterStartWorker(&context_id, std::move(task_info.task));
  }

  pending_registrations_.erase(extension_id);
}

void ServiceWorkerTaskQueue::DidUnregisterServiceWorker(
    const ExtensionId& extension_id,
    bool success) {
  // TODO(lazyboy): Handle success = false case.
}

}  // namespace extensions
