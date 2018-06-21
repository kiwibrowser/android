// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SESSION_CLEANUP_CHANNEL_ID_STORE_H_
#define SERVICES_NETWORK_SESSION_CLEANUP_CHANNEL_ID_STORE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/ssl/default_channel_id_store.h"

class GURL;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {

// Implements a PersistentStore that keeps an in-memory map of channel IDs, and
// allows deletion of channel IDs using the DeleteChannelIDPredicate. This is
// used to clear channel IDs with session-only policy at the end of a session.
class COMPONENT_EXPORT(NETWORK_SERVICE) SessionCleanupChannelIDStore
    : public net::DefaultChannelIDStore::PersistentStore {
 public:
  // Returns true if the channel ID for the URL should be deleted.
  using DeleteChannelIDPredicate = base::RepeatingCallback<bool(const GURL&)>;

  // Create or open persistent store in file |path|. All I/O tasks are performed
  // in background using |background_task_runner|.
  SessionCleanupChannelIDStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  // net::DefaultChannelIDStore::PersistentStore:
  void Load(const LoadedCallback& loaded_callback) override;
  void AddChannelID(
      const net::DefaultChannelIDStore::ChannelID& channel_id) override;
  void DeleteChannelID(
      const net::DefaultChannelIDStore::ChannelID& channel_id) override;
  void Flush() override;
  void SetForceKeepSessionState() override;

  // Should be called at the end of a session. Deletes all channel IDs that
  // |delete_channel_id_predicate| returns true for.
  void DeleteSessionChannelIDs(
      DeleteChannelIDPredicate delete_channel_id_predicate);

 protected:
  ~SessionCleanupChannelIDStore() override;

 private:
  using ChannelIDVector =
      std::vector<std::unique_ptr<net::DefaultChannelIDStore::ChannelID>>;

  void OnLoad(const LoadedCallback& loaded_callback,
              std::unique_ptr<ChannelIDVector> channel_ids);

  scoped_refptr<net::SQLiteChannelIDStore> persistent_store_;
  // Cache of server identifiers we have channel IDs stored for.
  std::set<std::string> server_identifiers_;
  // When set to true, DeleteSessionChannelIDs will be a no-op, and all channel
  // IDs will be kept.
  bool force_keep_session_state_ = false;

  DISALLOW_COPY_AND_ASSIGN(SessionCleanupChannelIDStore);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SESSION_CLEANUP_CHANNEL_ID_STORE_H_
