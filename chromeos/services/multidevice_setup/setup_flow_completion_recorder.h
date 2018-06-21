// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_SETUP_FLOW_COMPLETION_RECORDER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_SETUP_FLOW_COMPLETION_RECORDER_H_

#include "base/optional.h"
#include "base/time/time.h"

namespace chromeos {

namespace multidevice_setup {

// Records time at which the logged in user completed the MultiDevice setup flow
// on this device.
class SetupFlowCompletionRecorder {
 public:
  virtual ~SetupFlowCompletionRecorder() = default;

  // If the logged in GAIA account has completed the MultiDevice setup flow on
  // this device, this returns the time at which the flow was completed.
  // Otherwise it returns base::nullopt.
  virtual base::Optional<base::Time> GetCompletionTimestamp() = 0;
  virtual void RecordCompletion() = 0;

 protected:
  SetupFlowCompletionRecorder() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SetupFlowCompletionRecorder);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_SETUP_FLOW_COMPLETION_RECORDER_H_
