// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/transport_list_model.h"

TransportListModel::TransportListModel() = default;
TransportListModel::~TransportListModel() {
  for (auto& observer : observer_list_)
    observer.OnModelDestroyed();
}

void TransportListModel::AppendTransport(AuthenticatorTransport transport) {
  transports_.push_back(transport);
  for (auto& observer : observer_list_)
    observer.OnTransportAppended();
}

void TransportListModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TransportListModel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
