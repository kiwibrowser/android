// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"

namespace chromeos {

namespace multidevice_setup {

FakeAccountStatusChangeDelegate::FakeAccountStatusChangeDelegate() = default;

FakeAccountStatusChangeDelegate::~FakeAccountStatusChangeDelegate() = default;

mojom::AccountStatusChangeDelegatePtr
FakeAccountStatusChangeDelegate::GenerateInterfacePtr() {
  mojom::AccountStatusChangeDelegatePtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void FakeAccountStatusChangeDelegate::OnPotentialHostExistsForNewUser() {
  ++num_new_user_events_handled_;
}

void FakeAccountStatusChangeDelegate::OnConnectedHostSwitchedForExistingUser() {
  ++num_existing_user_host_switched_events_handled_;
}

void FakeAccountStatusChangeDelegate::OnNewChromebookAddedForExistingUser() {
  ++num_existing_user_chromebook_added_events_handled_;
}

}  // namespace multidevice_setup

}  // namespace chromeos
