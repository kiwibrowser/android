// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_api_component_factory.h"

#include "components/sync/driver/model_associator.h"
#include "components/sync/model/change_processor.h"

namespace syncer {

SyncApiComponentFactory::SyncComponents::SyncComponents() = default;

SyncApiComponentFactory::SyncComponents::SyncComponents(SyncComponents&&) =
    default;

SyncApiComponentFactory::SyncComponents::~SyncComponents() = default;

}  // namespace syncer
