// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {
// static
SetupFlowCompletionRecorderImpl::Factory*
    SetupFlowCompletionRecorderImpl::Factory::test_factory_ = nullptr;

// static
SetupFlowCompletionRecorderImpl::Factory*
SetupFlowCompletionRecorderImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<SetupFlowCompletionRecorderImpl::Factory> factory;
  return factory.get();
}

// static
void SetupFlowCompletionRecorderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SetupFlowCompletionRecorderImpl::Factory::~Factory() = default;

std::unique_ptr<SetupFlowCompletionRecorder>
SetupFlowCompletionRecorderImpl::Factory::BuildInstance(
    PrefService* pref_service,
    base::Clock* clock) {
  return base::WrapUnique(
      new SetupFlowCompletionRecorderImpl(pref_service, clock));
}

// static
const char SetupFlowCompletionRecorderImpl::kSetupFlowCompletedPrefName[] =
    "multidevice_setup.setup_flow_completed";

// static
void SetupFlowCompletionRecorderImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(kSetupFlowCompletedPrefName, 0);
}

SetupFlowCompletionRecorderImpl::~SetupFlowCompletionRecorderImpl() = default;

SetupFlowCompletionRecorderImpl::SetupFlowCompletionRecorderImpl(
    PrefService* pref_service,
    base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {}

base::Optional<base::Time>
SetupFlowCompletionRecorderImpl::GetCompletionTimestamp() {
  if (pref_service_->GetInt64(kSetupFlowCompletedPrefName) > 0)
    return base::Time::FromJavaTime(
        pref_service_->GetInt64(kSetupFlowCompletedPrefName));
  return base::nullopt;
}

void SetupFlowCompletionRecorderImpl::RecordCompletion() {
  pref_service_->SetInt64(kSetupFlowCompletedPrefName,
                          clock_->Now().ToJavaTime());
}

}  // namespace multidevice_setup

}  // namespace chromeos
