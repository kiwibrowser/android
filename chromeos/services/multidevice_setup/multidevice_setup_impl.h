// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_

#include <memory>

#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace chromeos {

namespace multidevice_setup {

// Concrete MultiDeviceSetup implementation.
class MultiDeviceSetupImpl : public MultiDeviceSetupBase {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<MultiDeviceSetupBase> BuildInstance();

   private:
    static Factory* test_factory_;
  };

  ~MultiDeviceSetupImpl() override;

 private:
  MultiDeviceSetupImpl();

  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojom::AccountStatusChangeDelegatePtr delegate,
      SetAccountStatusChangeDelegateCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  mojom::AccountStatusChangeDelegatePtr delegate_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_IMPL_H_
