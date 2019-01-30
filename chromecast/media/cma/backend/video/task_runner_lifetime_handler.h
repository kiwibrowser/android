// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_TASK_RUNNER_LIFETIME_HANDLER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_TASK_RUNNER_LIFETIME_HANDLER_H_

namespace chromecast {
class TaskRunnerImpl;

namespace media {

// This helper class is used to handle the lifetime of a task runner that needs
// to stay alive for a set amount of time while other compontents are using it.
class TaskRunnerLifetimeHandler {
 public:
  // Sets the task runner whose lifetime this class will handle.
  // TaskRunnerLifetimeHandler will create a handle to this task_runner, and
  // keep the handle alive until ResetTaskRunnerHandle is called.
  static void SetTaskRunnerHandle(const TaskRunnerImpl* task_runner_in);

  // Resets the task runner handle.
  static void ResetTaskRunnerHandle();
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_TASK_RUNNER_LIFETIME_HANDLER_H_
