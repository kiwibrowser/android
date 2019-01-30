// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_QUOTA_POLICY_COOKIE_STORE_H_
#define CONTENT_BROWSER_NET_QUOTA_POLICY_COOKIE_STORE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "services/network/session_cleanup_cookie_store.h"

namespace storage {
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

// Implements a PersistentCookieStore that deletes session cookies on
// shutdown. For documentation about the actual member functions consult the
// parent class |net::CookieMonster::PersistentCookieStore|. If provided, a
// |SpecialStoragePolicy| is consulted when the SQLite database is closed to
// decide which cookies to keep.
class CONTENT_EXPORT QuotaPolicyCookieStore
    : public network::SessionCleanupCookieStore {
 public:
  // Wraps the passed-in |cookie_store|.
  QuotaPolicyCookieStore(
      const scoped_refptr<net::SQLitePersistentCookieStore>& cookie_store,
      storage::SpecialStoragePolicy* special_storage_policy);

 private:
  ~QuotaPolicyCookieStore() override;

  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(QuotaPolicyCookieStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_QUOTA_POLICY_COOKIE_STORE_H_
