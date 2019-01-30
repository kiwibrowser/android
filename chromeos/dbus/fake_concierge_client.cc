// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_concierge_client.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeConciergeClient::FakeConciergeClient() {
  InitializeProtoResponses();
}
FakeConciergeClient::~FakeConciergeClient() = default;

// ConciergeClient override.
void FakeConciergeClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

// ConciergeClient override.
void FakeConciergeClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// ConciergeClient override.
bool FakeConciergeClient::IsContainerStartupFailedSignalConnected() {
  return is_container_startup_failed_signal_connected_;
}

void FakeConciergeClient::CreateDiskImage(
    const vm_tools::concierge::CreateDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback) {
  create_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_disk_image_response_));
}

void FakeConciergeClient::DestroyDiskImage(
    const vm_tools::concierge::DestroyDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
        callback) {
  destroy_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), destroy_disk_image_response_));
}

void FakeConciergeClient::ListVmDisks(
    const vm_tools::concierge::ListVmDisksRequest& request,
    DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse> callback) {
  list_vm_disks_called_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_vm_disks_response_));
}

void FakeConciergeClient::StartTerminaVm(
    const vm_tools::concierge::StartVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::StartVmResponse> callback) {
  start_termina_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_));
}

void FakeConciergeClient::StopVm(
    const vm_tools::concierge::StopVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback) {
  stop_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), stop_vm_response_));
}

void FakeConciergeClient::StartContainer(
    const vm_tools::concierge::StartContainerRequest& request,
    DBusMethodCallback<vm_tools::concierge::StartContainerResponse> callback) {
  start_container_called_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), start_container_response_));
}

void FakeConciergeClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeConciergeClient::GetContainerSshKeys(
    const vm_tools::concierge::ContainerSshKeysRequest& request,
    DBusMethodCallback<vm_tools::concierge::ContainerSshKeysResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), container_ssh_keys_response_));
}

void FakeConciergeClient::InitializeProtoResponses() {
  create_disk_image_response_.Clear();
  create_disk_image_response_.set_status(
      vm_tools::concierge::DISK_STATUS_CREATED);
  create_disk_image_response_.set_disk_path("foo");

  destroy_disk_image_response_.Clear();
  destroy_disk_image_response_.set_status(
      vm_tools::concierge::DISK_STATUS_DESTROYED);

  list_vm_disks_response_.Clear();
  list_vm_disks_response_.set_success(true);

  start_vm_response_.Clear();
  start_vm_response_.set_success(true);

  stop_vm_response_.Clear();
  stop_vm_response_.set_success(true);

  start_container_response_.Clear();
  start_container_response_.set_status(
      vm_tools::concierge::CONTAINER_STATUS_RUNNING);

  container_ssh_keys_response_.Clear();
}

}  // namespace chromeos
