// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier.h"

#include "base/logging.h"
#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace multidevice_setup {

AccountStatusChangeDelegateNotifier::AccountStatusChangeDelegateNotifier() =
    default;

AccountStatusChangeDelegateNotifier::~AccountStatusChangeDelegateNotifier() =
    default;

void AccountStatusChangeDelegateNotifier::SetAccountStatusChangeDelegatePtr(
    mojom::AccountStatusChangeDelegatePtr delegate_ptr) {
  if (delegate_ptr_.is_bound()) {
    PA_LOG(ERROR) << "AccountStatusChangeDelegateNotifier::"
                  << "SetAccountStatusChangeDelegatePtr(): Tried to set "
                  << "delegate, but one already existed.";
    NOTREACHED();
  }

  delegate_ptr_ = std::move(delegate_ptr);
  OnDelegateSet();
}

// No default implementation.
void AccountStatusChangeDelegateNotifier::OnDelegateSet() {}

void AccountStatusChangeDelegateNotifier::FlushForTesting() {
  if (delegate_ptr_)
    delegate_ptr_.FlushForTesting();
}

}  // namespace multidevice_setup

}  // namespace chromeos
