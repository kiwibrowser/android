// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

namespace syncer {
class SyncClient;
}

namespace sync_sessions {

// Overrides LoadModels to check if history sync is allowed by policy.
class SessionModelTypeController : public syncer::ModelTypeController {
 public:
  SessionModelTypeController(
      syncer::SyncClient* sync_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& model_thread,
      const std::string& history_disabled_pref_name);
  ~SessionModelTypeController() override;

  // DataTypeController overrides.
  bool ReadyForStart() const override;

 private:
  void OnSavingBrowserHistoryPrefChanged();

  // Name of the pref that indicates whether saving history is disabled.
  const std::string history_disabled_pref_name_;

  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SessionModelTypeController);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_
