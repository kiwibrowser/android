// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cicerone_client.h"

#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeCiceroneClient::FakeCiceroneClient() {
  launch_container_application_response_.Clear();
  launch_container_application_response_.set_success(true);

  container_app_icon_response_.Clear();
}
FakeCiceroneClient::~FakeCiceroneClient() = default;

void FakeCiceroneClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCiceroneClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeCiceroneClient::IsContainerStartedSignalConnected() {
  return is_container_started_signal_connected_;
}

bool FakeCiceroneClient::IsContainerShutdownSignalConnected() {
  return is_container_shutdown_signal_connected_;
}

void FakeCiceroneClient::LaunchContainerApplication(
    const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
    DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                launch_container_application_response_));
}

void FakeCiceroneClient::GetContainerAppIcons(
    const vm_tools::cicerone::ContainerAppIconRequest& request,
    DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), container_app_icon_response_));
}

void FakeCiceroneClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace chromeos
