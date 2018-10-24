// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/edk/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {
namespace service_worker_dispatcher_host_unittest {

static void SaveStatusCallback(bool* called,
                               ServiceWorkerStatusCode* out,
                               ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

struct RemoteProviderInfo {
  mojom::ServiceWorkerContainerHostAssociatedPtr host_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest client_request;
};

std::unique_ptr<ServiceWorkerNavigationHandleCore> CreateNavigationHandleCore(
    ServiceWorkerContextWrapper* context_wrapper) {
  std::unique_ptr<ServiceWorkerNavigationHandleCore> navigation_handle_core;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          [](ServiceWorkerContextWrapper* wrapper) {
            return std::make_unique<ServiceWorkerNavigationHandleCore>(nullptr,
                                                                       wrapper);
          },
          base::RetainedRef(context_wrapper)),
      base::Bind(
          [](std::unique_ptr<ServiceWorkerNavigationHandleCore>* dest,
             std::unique_ptr<ServiceWorkerNavigationHandleCore> src) {
            *dest = std::move(src);
          },
          &navigation_handle_core));
  base::RunLoop().RunUntilIdle();
  return navigation_handle_core;
}

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  ServiceWorkerDispatcherHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    Initialize(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
    mojo::edk::SetDefaultProcessErrorCallback(base::BindRepeating(
        &ServiceWorkerDispatcherHostTest::OnMojoError, base::Unretained(this)));
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
    mojo::edk::SetDefaultProcessErrorCallback(
        mojo::edk::ProcessErrorCallback());
  }

  void OnMojoError(const std::string& error) { bad_messages_.push_back(error); }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }

  void Initialize(std::unique_ptr<EmbeddedWorkerTestHelper> helper) {
    helper_.reset(helper.release());
    // Replace the default dispatcher host.
    int process_id = helper_->mock_render_process_id();
    dispatcher_host_.reset(
        new ServiceWorkerDispatcherHost(context_wrapper(), process_id));
  }

  void SetUpRegistration(const GURL& scope, const GURL& script_url) {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    registration_ =
        new ServiceWorkerRegistration(options, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(), script_url, 1L,
                                        context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);

    // Make the registration findable via storage functions.
    context()->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();
    bool called = false;
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
    context()->storage()->StoreRegistration(
        registration_.get(), version_.get(),
        base::BindOnce(&SaveStatusCallback, &called, &status));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
    EXPECT_EQ(SERVICE_WORKER_OK, status);
  }

  RemoteProviderInfo SendProviderCreated(
      ServiceWorkerProviderHostInfo host_info) {
    DCHECK(!host_info.host_request.is_pending());
    DCHECK(!host_info.client_ptr_info.is_valid());

    RemoteProviderInfo remote_info;
    mojom::ServiceWorkerContainerAssociatedPtrInfo client;
    remote_info.client_request = mojo::MakeRequest(&client);
    host_info.host_request = mojo::MakeRequest(&remote_info.host_ptr);
    host_info.client_ptr_info = std::move(client);

    mojom::ServiceWorkerDispatcherHostAssociatedPtr ptr;
    dispatcher_host_->AddBinding(
        mojo::MakeRequestAssociatedWithDedicatedPipe(&ptr));
    ptr->OnProviderCreated(std::move(host_info));
    base::RunLoop().RunUntilIdle();
    return remote_info;
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::unique_ptr<ServiceWorkerDispatcherHost, BrowserThread::DeleteOnIOThread>
      dispatcher_host_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  std::vector<std::string> bad_messages_;
};

TEST_F(ServiceWorkerDispatcherHostTest, ProviderCreatedAndDestroyed) {
  int process_id = helper_->mock_render_process_id();
  std::unique_ptr<ServiceWorkerNavigationHandleCore> navigation_handle_core;

  {
    // Prepare the first navigation handle to create provider host.
    const int kProviderId = -2;
    navigation_handle_core =
        CreateNavigationHandleCore(helper_->context_wrapper());
    ASSERT_TRUE(navigation_handle_core);
    base::WeakPtr<ServiceWorkerProviderHost> host =
        ServiceWorkerProviderHost::PreCreateNavigationHost(
            context()->AsWeakPtr(), true /* are_ancestors_secure */,
            base::RepeatingCallback<WebContents*(void)>());
    EXPECT_EQ(kProviderId, host->provider_id());
    ServiceWorkerProviderHostInfo host_info(kProviderId, 1 /* route_id */,
                                            host->provider_type(),
                                            host->is_parent_frame_secure());
    navigation_handle_core->DidPreCreateProviderHost(kProviderId);
    RemoteProviderInfo remote_provider =
        SendProviderCreated(std::move(host_info));
    EXPECT_TRUE(context()->GetProviderHost(process_id, kProviderId));

    // Releasing the interface pointer destroys the counterpart.
    remote_provider.host_ptr.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(context()->GetProviderHost(process_id, kProviderId));
  }

  {
    // Two with the same ID should be seen as a bad message.
    const int kProviderId = 99;
    RemoteProviderInfo remote_provider_1 =
        SendProviderCreated(ServiceWorkerProviderHostInfo(
            kProviderId, 1 /* route_id */,
            blink::mojom::ServiceWorkerProviderType::kForWindow,
            true /* is_parent_frame_secure */));
    EXPECT_TRUE(bad_messages_.empty());
    RemoteProviderInfo remote_provider_2 =
        SendProviderCreated(ServiceWorkerProviderHostInfo(
            kProviderId, 1 /* route_id */,
            blink::mojom::ServiceWorkerProviderType::kForWindow,
            true /* is_parent_frame_secure */));
    ASSERT_EQ(1u, bad_messages_.size());
    EXPECT_EQ("SWDH_PROVIDER_CREATED_DUPLICATE_ID", bad_messages_[0]);
  }

  {
    // Prepare another navigation handle to create another provider host.
    const int kProviderId = -3;
    navigation_handle_core =
        CreateNavigationHandleCore(helper_->context_wrapper());
    base::WeakPtr<ServiceWorkerProviderHost> host =
        ServiceWorkerProviderHost::PreCreateNavigationHost(
            context()->AsWeakPtr(), true /* are_ancestors_secure */,
            base::RepeatingCallback<WebContents*(void)>());
    EXPECT_EQ(kProviderId, host->provider_id());
    ServiceWorkerProviderHostInfo host_info(kProviderId, 2 /* route_id */,
                                            host->provider_type(),
                                            host->is_parent_frame_secure());
    navigation_handle_core->DidPreCreateProviderHost(kProviderId);
    RemoteProviderInfo remote_provider =
        SendProviderCreated(std::move(host_info));
    EXPECT_TRUE(context()->GetProviderHost(process_id, kProviderId));

    // Simulate that the corresponding renderer process died.
    dispatcher_host_->RenderProcessExited(nullptr /* host */,
                                          ChildProcessTerminationInfo());
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(context()->GetProviderHost(process_id, kProviderId));
  }
}

TEST_F(ServiceWorkerDispatcherHostTest, CleanupOnRendererCrash) {
  GURL pattern = GURL("https://www.example.com/");
  GURL script_url = GURL("https://www.example.com/service_worker.js");
  int process_id = helper_->mock_render_process_id();

  const int64_t kProviderId = 99;
  ServiceWorkerProviderHostInfo host_info_1(
      kProviderId, MSG_ROUTING_NONE,
      blink::mojom::ServiceWorkerProviderType::kForWindow,
      true /* is_parent_frame_secure */);
  RemoteProviderInfo remote_provider_1 =
      SendProviderCreated(std::move(host_info_1));
  ServiceWorkerProviderHost* provider_host = context()->GetProviderHost(
      helper_->mock_render_process_id(), kProviderId);
  SetUpRegistration(pattern, script_url);
  EXPECT_EQ(kProviderId, provider_host->provider_id());

  // Start up the worker.
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_ABORT;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::BindOnce(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(context()->GetProviderHost(process_id, kProviderId));
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Simulate that the corresponding renderer process died.
  dispatcher_host_->RenderProcessExited(nullptr /* host */,
                                        ChildProcessTerminationInfo());
  base::RunLoop().RunUntilIdle();
  // The dispatcher host should have removed the provider host.
  EXPECT_FALSE(context()->GetProviderHost(process_id, kProviderId));
  // The EmbeddedWorkerInstance should still think it is running, since it will
  // clean itself up when its Mojo connection to the renderer breaks.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Simulate that the browser process reused the dispatcher host due to reuse
  // of the RenderProcessHost for another new renderer process, then the new
  // renderer process creates a provider with the same |kProviderId|. Since the
  // dispatcher host already cleaned up the old provider host, the new one won't
  // complain.
  ServiceWorkerProviderHostInfo host_info_2(
      kProviderId, MSG_ROUTING_NONE,
      blink::mojom::ServiceWorkerProviderType::kForWindow,
      true /* is_parent_frame_secure */);
  RemoteProviderInfo remote_provider_2 =
      SendProviderCreated(std::move(host_info_2));
  EXPECT_TRUE(bad_messages_.empty());
}

}  // namespace service_worker_dispatcher_host_unittest
}  // namespace content
