// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_SETUP_FLOW_COMPLETION_RECORDER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_SETUP_FLOW_COMPLETION_RECORDER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder.h"

namespace chromeos {

namespace multidevice_setup {

class FakeSetupFlowCompletionRecorder : public SetupFlowCompletionRecorder {
 public:
  FakeSetupFlowCompletionRecorder();
  ~FakeSetupFlowCompletionRecorder() override;

  void set_current_time(base::Time current_time) {
    current_time_ = current_time;
  }

 private:
  // SetupFlowCompletionRecorder:
  base::Optional<base::Time> GetCompletionTimestamp() override;
  void RecordCompletion() override;

  base::Optional<base::Time> completion_time_;
  base::Time current_time_;
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_SETUP_FLOW_COMPLETION_RECORDER_H_
