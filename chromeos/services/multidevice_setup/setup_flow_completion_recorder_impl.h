// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_SETUP_FLOW_COMPLETION_RECORDER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_SETUP_FLOW_COMPLETION_RECORDER_IMPL_H_

#include <memory>

#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}

namespace chromeos {

namespace multidevice_setup {

// Concrete SetupFlowCompletionRecorder implementation.
class SetupFlowCompletionRecorderImpl : public SetupFlowCompletionRecorder {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<SetupFlowCompletionRecorder> BuildInstance(
        PrefService* pref_service,
        base::Clock* clock);

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~SetupFlowCompletionRecorderImpl() override;

  base::Optional<base::Time> GetCompletionTimestamp() override;
  void RecordCompletion() override;

 private:
  friend class SetupFlowCompletionRecorderImplTest;

  static const char kSetupFlowCompletedPrefName[];

  SetupFlowCompletionRecorderImpl(PrefService* pref_service,
                                  base::Clock* clock);

  PrefService* pref_service_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlowCompletionRecorderImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_SETUP_FLOW_COMPLETION_RECORDER_IMPL_H_
