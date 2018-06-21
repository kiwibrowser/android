// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_TRANSPORT_LIST_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_TRANSPORT_LIST_MODEL_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"

// Enumerates transports over which a Security Key can be reached.
enum class AuthenticatorTransport {
  kBluetoothLowEnergy,
  kUsb,
  kNearFieldCommunication,
  kInternal,
  kCloudAssistedBluetoothLowEnergy
};

// An observable list of transports that are supported on the platform.
class TransportListModel {
 public:
  class Observer {
   public:
    // Called just before the model is destructed.
    virtual void OnModelDestroyed() = 0;

    // Called when a new transport is added to the end of the list.
    virtual void OnTransportAppended() {}
  };

  TransportListModel();
  ~TransportListModel();

  // Appends |transport| at the end of the list.
  void AppendTransport(AuthenticatorTransport transport);

  // The |observer| must either outlive |this|, or unregister on destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::vector<AuthenticatorTransport>& transports() const {
    return transports_;
  }

 private:
  std::vector<AuthenticatorTransport> transports_;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(TransportListModel);
};

#endif  // CHROME_BROWSER_WEBAUTHN_TRANSPORT_LIST_MODEL_H_
