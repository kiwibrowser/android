// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_model_type_controller.h"

#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"

namespace sync_sessions {

SessionModelTypeController::SessionModelTypeController(
    syncer::SyncClient* sync_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& model_thread,
    const std::string& history_disabled_pref_name)
    : ModelTypeController(syncer::SESSIONS, sync_client, model_thread),
      history_disabled_pref_name_(history_disabled_pref_name) {
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      history_disabled_pref_name_,
      base::BindRepeating(
          &SessionModelTypeController::OnSavingBrowserHistoryPrefChanged,
          base::AsWeakPtr(this)));
}

SessionModelTypeController::~SessionModelTypeController() {}

bool SessionModelTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return !sync_client()->GetPrefService()->GetBoolean(
      history_disabled_pref_name_);
}

void SessionModelTypeController::OnSavingBrowserHistoryPrefChanged() {
  DCHECK(CalledOnValidThread());
  if (!sync_client()->GetPrefService()->GetBoolean(
          history_disabled_pref_name_)) {
    return;
  }

  // If history and tabs persistence is turned off then generate an
  // unrecoverable error. SESSIONS won't be a registered type on the next
  // Chrome restart.
  if (state() != NOT_RUNNING && state() != STOPPING) {
    ReportModelError(
        syncer::SyncError::DATATYPE_POLICY_ERROR,
        {FROM_HERE, "History and tab saving is now disabled by policy."});
  }
}

}  // namespace sync_sessions
