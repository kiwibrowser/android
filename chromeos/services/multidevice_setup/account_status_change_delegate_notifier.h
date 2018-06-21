// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// Notifies the delegate of MultiDeviceSetup for each of the following changes:
// (1) a potential host is found for someone who has not gone through the setup
//     flow before,
// (2) the host has switched for someone who has, or
// (3) a new Chromebook has been added to an account for someone who has.
class AccountStatusChangeDelegateNotifier {
 public:
  virtual ~AccountStatusChangeDelegateNotifier();

  void SetAccountStatusChangeDelegatePtr(
      mojom::AccountStatusChangeDelegatePtr delegate_ptr);

 protected:
  AccountStatusChangeDelegateNotifier();

  // Derived classes should override this function to be alerted when
  // SetAccountStatusChangeDelegatePtr() is called.
  virtual void OnDelegateSet();

  mojom::AccountStatusChangeDelegate* delegate() { return delegate_ptr_.get(); }

 private:
  friend class MultiDeviceSetupAccountStatusChangeDelegateNotifierTest;

  void FlushForTesting();

  mojom::AccountStatusChangeDelegatePtr delegate_ptr_;

  DISALLOW_COPY_AND_ASSIGN(AccountStatusChangeDelegateNotifier);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
