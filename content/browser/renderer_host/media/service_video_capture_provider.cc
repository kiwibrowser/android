// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include "content/browser/gpu/gpu_client_impl.h"
#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/browser/renderer_host/media/video_capture_dependencies.h"
#include "content/browser/renderer_host/media/video_capture_factory_delegate.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"

namespace {

class ServiceConnectorImpl
    : public content::ServiceVideoCaptureProvider::ServiceConnector {
 public:
  ServiceConnectorImpl() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // In unit test environments, there may not be any connector.
    auto* connection = content::ServiceManagerConnection::GetForProcess();
    if (!connection)
      return;
    auto* connector = connection->GetConnector();
    if (!connector)
      return;
    connector_ = connector->Clone();
  }

  void BindFactoryProvider(
      video_capture::mojom::DeviceFactoryProviderPtr* provider) override {
    if (!connector_) {
      CHECK(false) << "Attempted to connect to the video capture service from "
                      "a process that does not provide a "
                      "ServiceManagerConnection";
    }
    connector_->BindInterface(video_capture::mojom::kServiceName, provider);
  }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
};

class DelegateToBrowserGpuServiceAcceleratorFactory
    : public video_capture::mojom::AcceleratorFactory {
 public:
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override {
    content::VideoCaptureDependencies::CreateJpegDecodeAccelerator(
        std::move(jda_request));
  }
  void CreateJpegEncodeAccelerator(
      media::mojom::JpegEncodeAcceleratorRequest jea_request) override {
    content::VideoCaptureDependencies::CreateJpegEncodeAccelerator(
        std::move(jea_request));
  }
};

std::unique_ptr<ui::mojom::GpuMemoryBufferFactory> CreateGpuClient() {
  const auto gpu_client_id =
      content::ChildProcessHostImpl::GenerateChildProcessUniqueId();
  return std::make_unique<content::GpuClientImpl>(gpu_client_id);
}

std::unique_ptr<video_capture::mojom::AcceleratorFactory>
CreateAcceleratorFactory() {
  return std::make_unique<DelegateToBrowserGpuServiceAcceleratorFactory>();
}

}  // anonymous namespace

namespace content {

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : ServiceVideoCaptureProvider(
          std::make_unique<ServiceConnectorImpl>(),
          base::BindRepeating(&CreateGpuClient),
          base::BindRepeating(&CreateAcceleratorFactory),
          std::move(emit_log_message_cb)) {}

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    std::unique_ptr<ServiceConnector> service_connector,
    CreateMemoryBufferFactoryCallback create_memory_buffer_factory_cb,
    CreateAcceleratorFactoryCallback create_accelerator_factory_cb,
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : service_connector_(std::move(service_connector)),
      create_memory_buffer_factory_cb_(
          std::move(create_memory_buffer_factory_cb)),
      create_accelerator_factory_cb_(std::move(create_accelerator_factory_cb)),
      emit_log_message_cb_(std::move(emit_log_message_cb)),
      usage_count_(0),
      launcher_has_connected_to_device_factory_(false),
      weak_ptr_factory_(this) {}

ServiceVideoCaptureProvider::~ServiceVideoCaptureProvider() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  UninitializeInternal(ReasonForUninitialize::kShutdown);
}

void ServiceVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  emit_log_message_cb_.Run("ServiceVideoCaptureProvider::GetDeviceInfosAsync");
  IncreaseUsageCount();
  LazyConnectToService();
  // Use a ScopedCallbackRunner to make sure that |result_callback| gets
  // invoked with an empty result in case that the service drops the request.
  device_factory_->GetDeviceInfos(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&ServiceVideoCaptureProvider::OnDeviceInfosReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(result_callback)),
      std::vector<media::VideoCaptureDeviceInfo>()));
}

std::unique_ptr<VideoCaptureDeviceLauncher>
ServiceVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return std::make_unique<ServiceVideoCaptureDeviceLauncher>(
      base::BindRepeating(&ServiceVideoCaptureProvider::ConnectToDeviceFactory,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ServiceVideoCaptureProvider::ConnectToDeviceFactory(
    std::unique_ptr<VideoCaptureFactoryDelegate>* out_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  IncreaseUsageCount();
  LazyConnectToService();
  launcher_has_connected_to_device_factory_ = true;
  *out_factory = std::make_unique<VideoCaptureFactoryDelegate>(
      &device_factory_,
      base::BindOnce(&ServiceVideoCaptureProvider::DecreaseUsageCount,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ServiceVideoCaptureProvider::LazyConnectToService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (device_factory_provider_.is_bound())
    return;

  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::BROWSER_CONNECTING_TO_SERVICE);
  if (time_of_last_uninitialize_ != base::TimeTicks()) {
    if (launcher_has_connected_to_device_factory_) {
      video_capture::uma::LogDurationUntilReconnectAfterCapture(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    } else {
      video_capture::uma::LogDurationUntilReconnectAfterEnumerationOnly(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    }
  }

  launcher_has_connected_to_device_factory_ = false;
  time_of_last_connect_ = base::TimeTicks::Now();

  video_capture::mojom::AcceleratorFactoryPtr accelerator_factory;
  ui::mojom::GpuMemoryBufferFactoryPtr memory_buffer_factory;
  mojo::MakeStrongBinding(create_accelerator_factory_cb_.Run(),
                          mojo::MakeRequest(&accelerator_factory));
  mojo::MakeStrongBinding(create_memory_buffer_factory_cb_.Run(),
                          mojo::MakeRequest(&memory_buffer_factory));
  service_connector_->BindFactoryProvider(&device_factory_provider_);
  device_factory_provider_->InjectGpuDependencies(
      std::move(memory_buffer_factory), std::move(accelerator_factory));
  device_factory_provider_->ConnectToDeviceFactory(
      mojo::MakeRequest(&device_factory_));
  // Unretained |this| is safe, because |this| owns |device_factory_|.
  device_factory_.set_connection_error_handler(base::BindOnce(
      &ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory,
      base::Unretained(this)));
}

void ServiceVideoCaptureProvider::OnDeviceInfosReceived(
    GetDeviceInfosCallback result_callback,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::ResetAndReturn(&result_callback).Run(infos);
  DecreaseUsageCount();
}

void ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  emit_log_message_cb_.Run(
      "ServiceVideoCaptureProvider::OnLostConnectionToDeviceFactory");
  // This may indicate that the video capture service has crashed. Uninitialize
  // here, so that a new connection will be established when clients try to
  // reconnect.
  UninitializeInternal(ReasonForUninitialize::kConnectionLost);
}

void ServiceVideoCaptureProvider::IncreaseUsageCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  usage_count_++;
}

void ServiceVideoCaptureProvider::DecreaseUsageCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  usage_count_--;
  DCHECK_GE(usage_count_, 0);
  if (usage_count_ == 0)
    UninitializeInternal(ReasonForUninitialize::kUnused);
}

void ServiceVideoCaptureProvider::UninitializeInternal(
    ReasonForUninitialize reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!device_factory_.is_bound()) {
    return;
  }
  base::TimeDelta duration_since_last_connect(base::TimeTicks::Now() -
                                              time_of_last_connect_);
  switch (reason) {
    case ReasonForUninitialize::kShutdown:
    case ReasonForUninitialize::kUnused:
      if (launcher_has_connected_to_device_factory_) {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_CAPTURE);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterCapture(
                duration_since_last_connect);
      } else {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_ENUMERATION_ONLY);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterEnumerationOnly(
                duration_since_last_connect);
      }
      break;
    case ReasonForUninitialize::kConnectionLost:
      video_capture::uma::LogVideoCaptureServiceEvent(
          video_capture::uma::BROWSER_LOST_CONNECTION_TO_SERVICE);
      video_capture::uma::LogDurationFromLastConnectToConnectionLost(
          duration_since_last_connect);
      break;
  }
  device_factory_.reset();
  device_factory_provider_.reset();
  time_of_last_uninitialize_ = base::TimeTicks::Now();
}

}  // namespace content
