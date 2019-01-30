// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_MODULE_LOAD_ATTEMPT_LOG_LISTENER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_MODULE_LOAD_ATTEMPT_LOG_LISTENER_WIN_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_types.h"
#include "chrome_elf/third_party_dlls/packed_list_format.h"

namespace base {
class SequencedTaskRunner;
}

// This class drains the log of module load attempt from Chrome ELF, and
// notifies its delegate for all modules that were blocked.
class ModuleLoadAttemptLogListener : public base::win::ObjectWatcher::Delegate {
 public:
  using OnNewModulesBlockedCallback = base::RepeatingCallback<void(
      std::vector<third_party_dlls::PackedListModule>&& blocked_modules)>;

  explicit ModuleLoadAttemptLogListener(
      OnNewModulesBlockedCallback on_new_modules_blocked_callback);
  ~ModuleLoadAttemptLogListener() override;

  // base::win::ObjectWatcher::Delegate:
  void OnObjectSignaled(HANDLE object) override;

 private:
  void StartDrainingLogs();

  void OnLogDrained(
      std::vector<third_party_dlls::PackedListModule>&& blocked_modules);

  // Invoked everytime the log is drained with the new blocked entries.
  OnNewModulesBlockedCallback on_new_modules_blocked_callback_;

  // The sequence in which the log is drained.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Must be before |object_watcher_|.
  base::WaitableEvent waitable_event_;

  // Watches |waitable_event_|.
  base::win::ObjectWatcher object_watcher_;

  base::WeakPtrFactory<ModuleLoadAttemptLogListener> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModuleLoadAttemptLogListener);
};

#endif  // CHROME_BROWSER_CONFLICTS_MODULE_LOAD_ATTEMPT_LOG_LISTENER_WIN_H_
