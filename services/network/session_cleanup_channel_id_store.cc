// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/session_cleanup_channel_id_store.h"

#include <list>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "url/gurl.h"

namespace network {

SessionCleanupChannelIDStore::SessionCleanupChannelIDStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : persistent_store_(
          new net::SQLiteChannelIDStore(path, background_task_runner)) {
  DCHECK(background_task_runner.get());
}

SessionCleanupChannelIDStore::~SessionCleanupChannelIDStore() {}

void SessionCleanupChannelIDStore::DeleteSessionChannelIDs(
    DeleteChannelIDPredicate delete_channel_id_predicate) {
  if (force_keep_session_state_ || !delete_channel_id_predicate)
    return;

  std::list<std::string> session_only_server_identifiers;
  for (const std::string& server_identifier : server_identifiers_) {
    GURL url(net::cookie_util::CookieOriginToURL(server_identifier, true));
    if (delete_channel_id_predicate.Run(url))
      session_only_server_identifiers.push_back(server_identifier);
  }
  persistent_store_->DeleteAllInList(session_only_server_identifiers);
}

void SessionCleanupChannelIDStore::Load(const LoadedCallback& loaded_callback) {
  persistent_store_->Load(base::BindRepeating(
      &SessionCleanupChannelIDStore::OnLoad, this, loaded_callback));
}

void SessionCleanupChannelIDStore::AddChannelID(
    const net::DefaultChannelIDStore::ChannelID& channel_id) {
  server_identifiers_.insert(channel_id.server_identifier());
  persistent_store_->AddChannelID(channel_id);
}

void SessionCleanupChannelIDStore::DeleteChannelID(
    const net::DefaultChannelIDStore::ChannelID& channel_id) {
  server_identifiers_.erase(channel_id.server_identifier());
  persistent_store_->DeleteChannelID(channel_id);
}

void SessionCleanupChannelIDStore::Flush() {
  persistent_store_->Flush();
}

void SessionCleanupChannelIDStore::SetForceKeepSessionState() {
  force_keep_session_state_ = true;
}

void SessionCleanupChannelIDStore::OnLoad(
    const LoadedCallback& loaded_callback,
    std::unique_ptr<ChannelIDVector> channel_ids) {
  for (const auto& channel_id : *channel_ids)
    server_identifiers_.insert(channel_id->server_identifier());
  loaded_callback.Run(std::move(channel_ids));
}

}  // namespace network
