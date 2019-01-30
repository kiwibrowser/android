// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_REQUEST_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_REQUEST_H_

#include <string>

#include "components/sync/model/model_error.h"

namespace syncer {

// The state passed from ModelTypeController to the delegate during DataType
// activation.
struct DataTypeActivationRequest {
  DataTypeActivationRequest();
  DataTypeActivationRequest(const DataTypeActivationRequest& request);
  DataTypeActivationRequest(DataTypeActivationRequest&& request);
  ~DataTypeActivationRequest();

  ModelErrorHandler error_handler;
  std::string authenticated_account_id;
  std::string cache_guid;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_REQUEST_H_
