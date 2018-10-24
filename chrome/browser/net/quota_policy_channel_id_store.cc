// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/quota_policy_channel_id_store.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "storage/browser/quota/special_storage_policy.h"

QuotaPolicyChannelIDStore::QuotaPolicyChannelIDStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy)
    : SessionCleanupChannelIDStore(path, background_task_runner),
      special_storage_policy_(special_storage_policy) {}

QuotaPolicyChannelIDStore::~QuotaPolicyChannelIDStore() {
  if (!special_storage_policy_.get() ||
      !special_storage_policy_->HasSessionOnlyOrigins()) {
    return;
  }

  DeleteSessionChannelIDs(
      base::BindRepeating(&storage::SpecialStoragePolicy::IsStorageSessionOnly,
                          special_storage_policy_));
}
