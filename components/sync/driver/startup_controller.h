// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_STARTUP_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_STARTUP_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"

namespace syncer {

class SyncPrefs;

// This class is used by ProfileSyncService to manage all logic and state
// pertaining to initialization of the SyncEngine.
class StartupController {
 public:
  enum class State {
    // Startup has not been triggered yet.
    NOT_STARTED,
    // Startup has been triggered but is deferred. The actual startup will
    // happen once the deferred delay expires (or when immediate startup is
    // requested, whichever happens first).
    STARTING_DEFERRED,
    // Startup has happened, i.e. |start_engine| has been run.
    STARTED
  };

  StartupController(
      const SyncPrefs* sync_prefs,
      base::RepeatingCallback<ModelTypeSet()> get_preferred_data_types,
      base::RepeatingCallback<bool()> can_start,
      base::RepeatingClosure start_engine);
  ~StartupController();

  // Starts up sync if it is requested by the user and preconditions are met.
  void TryStart();

  // Same as TryStart() above, but bypasses deferred startup and the first setup
  // complete check.
  void TryStartImmediately();

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
  // It is expected that |type| is a currently active datatype.
  void OnDataTypeRequestsSyncStartup(ModelType type);

  // Prepares this object for a new attempt to start sync, forgetting
  // whether or not preconditions were previously met.
  // NOTE: This resets internal state managed by this class, but does not
  // touch values that are explicitly set and reset by higher layers to
  // tell this class whether a setup UI dialog is being shown to the user.
  // See setup_in_progress_.
  void Reset();

  // Sets the setup in progress flag and tries to start sync if it's true.
  void SetSetupInProgress(bool setup_in_progress);

  State GetState() const;

  bool IsSetupInProgress() const { return setup_in_progress_; }
  base::Time start_engine_time() const { return start_engine_time_; }

 private:
  enum StartUpDeferredOption { STARTUP_DEFERRED, STARTUP_IMMEDIATE };

  void StartUp(StartUpDeferredOption deferred_option);
  void OnFallbackStartupTimerExpired();

  // Records time spent in deferred state with UMA histograms.
  void RecordTimeDeferred();

  const SyncPrefs* sync_prefs_;

  const base::RepeatingCallback<ModelTypeSet()>
      get_preferred_data_types_callback_;

  // A function that can be invoked repeatedly to determine whether sync can be
  // started. |start_engine_| should not be invoked unless this returns true.
  const base::RepeatingCallback<bool()> can_start_callback_;

  // The callback we invoke when it's time to call expensive
  // startup routines for the sync engine.
  const base::RepeatingClosure start_engine_callback_;

  // If true, will bypass the FirstSetupComplete check when triggering sync
  // startup. Set in TryStartImmediately.
  bool bypass_setup_complete_;

  // True if we should start sync ASAP because either a data type has
  // requested it, or TryStartImmediately was called, or our deferred startup
  // timer has expired.
  bool received_start_request_;

  // The time that StartUp() is called. This is used to calculate time spent
  // in the deferred state; that is, after StartUp and before invoking the
  // start_engine_ callback. If this is non-null, then a (possibly deferred)
  // startup has been triggered.
  base::Time start_up_time_;

  // If |true|, there is setup UI visible so we should not start downloading
  // data types.
  // Note: this is explicitly controlled by higher layers (UI) and is meant to
  // reflect what the UI claims the setup state to be. Therefore, only set this
  // due to explicit requests to do so via SetSetupInProgress.
  bool setup_in_progress_;

  // The time at which we invoked the |start_engine_| callback. If this is
  // non-null, then |start_engine_| shouldn't be called again.
  base::Time start_engine_time_;

  base::WeakPtrFactory<StartupController> weak_factory_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_STARTUP_CONTROLLER_H_
