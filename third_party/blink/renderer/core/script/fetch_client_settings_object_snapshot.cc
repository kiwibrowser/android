// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/fetch_client_settings_object_snapshot.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    ExecutionContext& execution_context)
    : FetchClientSettingsObjectSnapshot(execution_context.BaseURL(),
                                        execution_context.GetSecurityOrigin(),
                                        execution_context.GetReferrerPolicy(),
                                        execution_context.OutgoingReferrer()) {}

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    const KURL& base_url,
    const scoped_refptr<const SecurityOrigin> security_origin,
    ReferrerPolicy referrer_policy,
    const String& outgoing_referrer)
    : base_url_(base_url),
      security_origin_(std::move(security_origin)),
      referrer_policy_(referrer_policy),
      outgoing_referrer_(outgoing_referrer) {}

}  // namespace blink
