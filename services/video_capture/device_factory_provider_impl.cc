// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_provider_impl.h"

#include "base/task_scheduler/post_task.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "services/ui/public/cpp/gpu/client_gpu_memory_buffer_manager.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"

namespace video_capture {

// Intended usage of this class is to instantiate on any sequence, and then
// operate and release the instance on the task runner exposed via
// GetTaskRunner() via WeakPtrs provided via GetWeakPtr(). To this end,
// GetTaskRunner() and GetWeakPtr() can be called from any sequence, typically
// the same as the one calling the constructor.
class DeviceFactoryProviderImpl::GpuDependenciesContext {
 public:
  GpuDependenciesContext() : weak_factory_for_gpu_io_thread_(this) {
    gpu_io_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::TaskPriority::BACKGROUND, base::MayBlock()});
  }

  ~GpuDependenciesContext() {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
  }

  base::WeakPtr<GpuDependenciesContext> GetWeakPtr() {
    return weak_factory_for_gpu_io_thread_.GetWeakPtr();
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return gpu_io_task_runner_;
  }

  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() {
    return gpu_memory_buffer_manager_.get();
  }

  void InjectGpuDependencies(
      ui::mojom::GpuMemoryBufferFactoryPtrInfo memory_buffer_factory_info,
      mojom::AcceleratorFactoryPtrInfo accelerator_factory_info) {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
    accelerator_factory_.Bind(std::move(accelerator_factory_info));

// Since the instance of ui::ClientGpuMemoryBufferManager seems kind of
// expensive, we only create it on platforms where it gets actually used.
#if defined(OS_CHROMEOS)
    ui::mojom::GpuMemoryBufferFactoryPtr memory_buffer_factory;
    memory_buffer_factory.Bind(std::move(memory_buffer_factory_info));
    gpu_memory_buffer_manager_ =
        std::make_unique<ui::ClientGpuMemoryBufferManager>(
            std::move(memory_buffer_factory));
#endif
  }

  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest request) {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
    if (!accelerator_factory_)
      return;
    accelerator_factory_->CreateJpegDecodeAccelerator(std::move(request));
  }

  void CreateJpegEncodeAccelerator(
      media::mojom::JpegEncodeAcceleratorRequest request) {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
    if (!accelerator_factory_)
      return;
    accelerator_factory_->CreateJpegEncodeAccelerator(std::move(request));
  }

 private:
  // Task runner for operating |accelerator_factory_| and
  // |gpu_memory_buffer_manager_| on. This must be a different thread from the
  // main service thread in order to avoid a deadlock during shutdown where
  // the main service thread joins a video capture device thread that, in turn,
  // will try to post the release of the jpeg decoder to the thread it is
  // operated on.
  scoped_refptr<base::SequencedTaskRunner> gpu_io_task_runner_;
  mojom::AcceleratorFactoryPtr accelerator_factory_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  base::WeakPtrFactory<GpuDependenciesContext> weak_factory_for_gpu_io_thread_;
};

DeviceFactoryProviderImpl::DeviceFactoryProviderImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref,
    base::Callback<void(float)> set_shutdown_delay_cb)
    : service_ref_(std::move(service_ref)),
      set_shutdown_delay_cb_(std::move(set_shutdown_delay_cb)) {}

DeviceFactoryProviderImpl::~DeviceFactoryProviderImpl() {
  factory_bindings_.CloseAllBindings();
  device_factory_.reset();
  gpu_dependencies_context_->GetTaskRunner()->DeleteSoon(
      FROM_HERE, std::move(gpu_dependencies_context_));
}

void DeviceFactoryProviderImpl::InjectGpuDependencies(
    ui::mojom::GpuMemoryBufferFactoryPtr memory_buffer_factory,
    mojom::AcceleratorFactoryPtr accelerator_factory) {
  LazyInitializeGpuDependenciesContext();
  gpu_dependencies_context_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuDependenciesContext::InjectGpuDependencies,
                                gpu_dependencies_context_->GetWeakPtr(),
                                memory_buffer_factory.PassInterface(),
                                accelerator_factory.PassInterface()));
}

void DeviceFactoryProviderImpl::ConnectToDeviceFactory(
    mojom::DeviceFactoryRequest request) {
  LazyInitializeDeviceFactory();
  factory_bindings_.AddBinding(device_factory_.get(), std::move(request));
}

void DeviceFactoryProviderImpl::SetShutdownDelayInSeconds(float seconds) {
  set_shutdown_delay_cb_.Run(seconds);
}

void DeviceFactoryProviderImpl::LazyInitializeGpuDependenciesContext() {
  if (!gpu_dependencies_context_)
    gpu_dependencies_context_ = std::make_unique<GpuDependenciesContext>();
}

void DeviceFactoryProviderImpl::LazyInitializeDeviceFactory() {
  if (device_factory_)
    return;

  LazyInitializeGpuDependenciesContext();

  // Create the platform-specific device factory.
  // The task runner passed to CreateFactory is used for things that need to
  // happen on a "UI thread equivalent", e.g. obtaining screen rotation on
  // Chrome OS.
  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::VideoCaptureDeviceFactory::CreateFactory(
          base::ThreadTaskRunnerHandle::Get(),
          gpu_dependencies_context_->GetGpuMemoryBufferManager(),
          base::BindRepeating(
              &GpuDependenciesContext::CreateJpegDecodeAccelerator,
              gpu_dependencies_context_->GetWeakPtr()),
          base::BindRepeating(
              &GpuDependenciesContext::CreateJpegEncodeAccelerator,
              gpu_dependencies_context_->GetWeakPtr()));
  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(media_device_factory));

  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      service_ref_->Clone(),
      std::make_unique<DeviceFactoryMediaToMojoAdapter>(
          service_ref_->Clone(), std::move(video_capture_system),
          base::BindRepeating(
              &GpuDependenciesContext::CreateJpegDecodeAccelerator,
              gpu_dependencies_context_->GetWeakPtr()),
          gpu_dependencies_context_->GetTaskRunner()));
}

}  // namespace video_capture
