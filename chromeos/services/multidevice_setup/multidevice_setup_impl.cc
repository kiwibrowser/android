// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace multidevice_setup {

// static
MultiDeviceSetupImpl::Factory* MultiDeviceSetupImpl::Factory::test_factory_ =
    nullptr;

// static
MultiDeviceSetupImpl::Factory* MultiDeviceSetupImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void MultiDeviceSetupImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupImpl::Factory::~Factory() = default;

std::unique_ptr<MultiDeviceSetupBase>
MultiDeviceSetupImpl::Factory::BuildInstance() {
  return base::WrapUnique(new MultiDeviceSetupImpl());
}

MultiDeviceSetupImpl::MultiDeviceSetupImpl() = default;

MultiDeviceSetupImpl::~MultiDeviceSetupImpl() = default;

void MultiDeviceSetupImpl::SetAccountStatusChangeDelegate(
    mojom::AccountStatusChangeDelegatePtr delegate,
    SetAccountStatusChangeDelegateCallback callback) {
  delegate_ = std::move(delegate);
  std::move(callback).Run();
}

void MultiDeviceSetupImpl::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    TriggerEventForDebuggingCallback callback) {
  PA_LOG(INFO) << "MultiDeviceSetupImpl::TriggerEventForDebugging(" << type
               << ") called.";

  if (!delegate_.is_bound()) {
    PA_LOG(ERROR) << "MultiDeviceSetupImpl::TriggerEventForDebugging(): No "
                  << "delgate has been set; cannot proceed.";
    std::move(callback).Run(false /* success */);
    return;
  }

  switch (type) {
    case mojom::EventTypeForDebugging::kNewUserPotentialHostExists:
      delegate_->OnPotentialHostExistsForNewUser();
      break;
    case mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched:
      delegate_->OnConnectedHostSwitchedForExistingUser();
      break;
    case mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded:
      delegate_->OnNewChromebookAddedForExistingUser();
      break;
    default:
      NOTREACHED();
  }

  std::move(callback).Run(true /* success */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
