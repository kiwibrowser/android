// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

class ConsentSyncBridge {
 public:
  ConsentSyncBridge() = default;
  virtual ~ConsentSyncBridge() = default;

  virtual void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) = 0;

  // Returns the delegate for the controller, i.e. sync integration point.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateOnUIThread() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_H_
