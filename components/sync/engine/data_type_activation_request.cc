// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_activation_request.h"

namespace syncer {

DataTypeActivationRequest::DataTypeActivationRequest() = default;

DataTypeActivationRequest::DataTypeActivationRequest(
    const DataTypeActivationRequest&) = default;

DataTypeActivationRequest::DataTypeActivationRequest(
    DataTypeActivationRequest&& request) = default;

DataTypeActivationRequest::~DataTypeActivationRequest() = default;

}  // namespace syncer
