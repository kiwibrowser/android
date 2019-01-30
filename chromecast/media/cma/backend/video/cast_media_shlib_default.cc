// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/cast_media_shlib.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/backend/video/task_runner_lifetime_handler.h"

namespace chromecast {
namespace media {

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  TaskRunnerLifetimeHandler::SetTaskRunnerHandle(
      static_cast<TaskRunnerImpl*>(params.task_runner));

  return new MediaPipelineBackendForMixer(params);
}

void CastMediaShlib::Finalize() {
  TaskRunnerLifetimeHandler::ResetTaskRunnerHandle();
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  return false;
}

double CastMediaShlib::GetMediaClockRate() {
  return 0.0;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return 0.0;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {
  *minimum_rate = 0.0;
  *maximum_rate = 1.0;
}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  return false;
}

}  // namespace media
}  // namespace chromecast
