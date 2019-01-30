// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_setup_flow_completion_recorder.h"

namespace chromeos {

namespace multidevice_setup {

FakeSetupFlowCompletionRecorder::FakeSetupFlowCompletionRecorder() = default;

FakeSetupFlowCompletionRecorder::~FakeSetupFlowCompletionRecorder() = default;

base::Optional<base::Time>
FakeSetupFlowCompletionRecorder::GetCompletionTimestamp() {
  return completion_time_;
}

void FakeSetupFlowCompletionRecorder::RecordCompletion() {
  completion_time_ = current_time_;
}

}  // namespace multidevice_setup

}  // namespace chromeos
