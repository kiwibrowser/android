// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_stop_metadata_fate.h"

#include "base/logging.h"

namespace syncer {

const char* SyncStopMetadataFateToString(SyncStopMetadataFate metadata_fate) {
  switch (metadata_fate) {
    case KEEP_METADATA:
      return "KEEP_METADATA";
    case CLEAR_METADATA:
      return "CLEAR_METADATA";
  }

  NOTREACHED() << "Invalid metadata fate " << static_cast<int>(metadata_fate);
  return "INVALID";
}

}  // namespace syncer
