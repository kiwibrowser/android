// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_OPT_OUT_STORE_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_OPT_OUT_STORE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/previews/core/blacklist_data.h"

namespace previews {

typedef base::OnceCallback<void(std::unique_ptr<BlacklistData>)>
    LoadBlackListCallback;

// PreviewsOptOutStore keeps opt out information for the previews.
// Ability to create multiple instances of the store as well as behavior of
// asynchronous operations when the object is being destroyed, before such
// operation finishes will depend on implementation. It is possible to issue
// multiple asynchronous operations in parallel and maintain ordering.
class PreviewsOptOutStore {
 public:
  virtual ~PreviewsOptOutStore() {}

  // Adds a new navigation to the store. |opt_out| is whether the user opted out
  // of the preview.
  virtual void AddEntry(bool opt_out,
                        const std::string& host_name,
                        int type,
                        base::Time now) = 0;

  // Asynchronously loads a map of host names to PreviewsBlackListItem for that
  // host from the store. And runs |callback| once loading is finished.
  virtual void LoadBlackList(std::unique_ptr<BlacklistData> blacklist_data,
                             LoadBlackListCallback callback) = 0;

  // Deletes all history in the store between |begin_time| and |end_time|.
  virtual void ClearBlackList(base::Time begin_time, base::Time end_time) = 0;
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_OPT_OUT_STORE_H_
