// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/concierge_client.h"

namespace chromeos {

// FakeConciergeClient is a light mock of ConciergeClient used for testing.
class CHROMEOS_EXPORT FakeConciergeClient : public ConciergeClient {
 public:
  FakeConciergeClient();
  ~FakeConciergeClient() override;

  // Adds an observer.
  void AddObserver(Observer* observer) override;

  // Removes an observer if added.
  void RemoveObserver(Observer* observer) override;

  // IsContainerStartupFailedSignalConnected must return true before
  // StartContainer is called.
  bool IsContainerStartupFailedSignalConnected() override;

  // Fake version of the method that creates a disk image for the Termina VM.
  // Sets create_disk_image_called_. |callback| is called after the method
  // call finishes.
  void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback)
      override;

  // Fake version of the method that destroys a Termina VM and removes its disk
  // image. Sets destroy_disk_image_called_. |callback| is called after the
  // method call finishes.
  void DestroyDiskImage(
      const vm_tools::concierge::DestroyDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
          callback) override;

  // Fake version of the method that lists Termina VM disks. Sets
  // list_vm_disks_called_. |callback| is called after the method call
  // finishes.
  void ListVmDisks(const vm_tools::concierge::ListVmDisksRequest& request,
                   DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
                       callback) override;

  // Fake version of the method that starts a Termina VM. Sets
  // start_termina_vm_called_. |callback| is called after the method call
  // finishes.
  void StartTerminaVm(const vm_tools::concierge::StartVmRequest& request,
                      DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                          callback) override;

  // Fake version of the method that stops the named Termina VM if it is
  // running. Sets stop_vm_called_. |callback| is called after the method
  // call finishes.
  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback)
      override;

  // Fake version of the method that starts a Container inside an existing
  // Termina VM. Sets start_container_called_. |callback| is called after the
  // method call finishes.
  void StartContainer(
      const vm_tools::concierge::StartContainerRequest& request,
      DBusMethodCallback<vm_tools::concierge::StartContainerResponse> callback)
      override;

  // Fake version of the method that waits for the Concierge service to be
  // availble.  |callback| is called after the method call finishes.
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Fake version of the method that fetches ssh key information.
  // |callback| is called after the method call finishes.
  void GetContainerSshKeys(
      const vm_tools::concierge::ContainerSshKeysRequest& request,
      DBusMethodCallback<vm_tools::concierge::ContainerSshKeysResponse>
          callback) override;

  // Indicates whether CreateDiskImage has been called
  bool create_disk_image_called() const { return create_disk_image_called_; }
  // Indicates whether DestroyDiskImage has been called
  bool destroy_disk_image_called() const { return destroy_disk_image_called_; }
  // Indicates whether ListVmDisks has been called
  bool list_vm_disks_called() const { return list_vm_disks_called_; }
  // Indicates whether StartTerminaVm has been called
  bool start_termina_vm_called() const { return start_termina_vm_called_; }
  // Indicates whether StopVm has been called
  bool stop_vm_called() const { return stop_vm_called_; }
  // Indicates whether StartContainer has been called
  bool start_container_called() const { return start_container_called_; }
  // Set ContainerStartupFailedSignalConnected state
  void set_container_startup_failed_signal_connected(bool connected) {
    is_container_startup_failed_signal_connected_ = connected;
  }

  void set_create_disk_image_response(
      const vm_tools::concierge::CreateDiskImageResponse&
          create_disk_image_response) {
    create_disk_image_response_ = create_disk_image_response;
  }
  void set_destroy_disk_image_response(
      const vm_tools::concierge::DestroyDiskImageResponse&
          destroy_disk_image_response) {
    destroy_disk_image_response_ = destroy_disk_image_response;
  }
  void set_list_vm_disks_response(
      const vm_tools::concierge::ListVmDisksResponse& list_vm_disks_response) {
    list_vm_disks_response_ = list_vm_disks_response;
  }
  void set_start_vm_response(
      const vm_tools::concierge::StartVmResponse& start_vm_response) {
    start_vm_response_ = start_vm_response;
  }
  void set_stop_vm_response(
      const vm_tools::concierge::StopVmResponse& stop_vm_response) {
    stop_vm_response_ = stop_vm_response;
  }
  void set_start_container_response(
      const vm_tools::concierge::StartContainerResponse&
          start_container_response) {
    start_container_response_ = start_container_response;
  }
  void set_container_ssh_keys_response(
      const vm_tools::concierge::ContainerSshKeysResponse&
          container_ssh_keys_response) {
    container_ssh_keys_response_ = container_ssh_keys_response;
  }

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  void InitializeProtoResponses();

  bool create_disk_image_called_ = false;
  bool destroy_disk_image_called_ = false;
  bool list_vm_disks_called_ = false;
  bool start_termina_vm_called_ = false;
  bool stop_vm_called_ = false;
  bool start_container_called_ = false;
  bool is_container_startup_failed_signal_connected_ = true;

  vm_tools::concierge::CreateDiskImageResponse create_disk_image_response_;
  vm_tools::concierge::DestroyDiskImageResponse destroy_disk_image_response_;
  vm_tools::concierge::ListVmDisksResponse list_vm_disks_response_;
  vm_tools::concierge::StartVmResponse start_vm_response_;
  vm_tools::concierge::StopVmResponse stop_vm_response_;
  vm_tools::concierge::StartContainerResponse start_container_response_;
  vm_tools::concierge::ContainerSshKeysResponse container_ssh_keys_response_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeConciergeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_
