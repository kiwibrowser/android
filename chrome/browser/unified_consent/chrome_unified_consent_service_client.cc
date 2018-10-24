// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/chrome_unified_consent_service_client.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

ChromeUnifiedConsentServiceClient::ChromeUnifiedConsentServiceClient(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

void ChromeUnifiedConsentServiceClient::SetAlternateErrorPagesEnabled(
    bool enabled) {
  pref_service_->SetBoolean(prefs::kAlternateErrorPagesEnabled, enabled);
}

void ChromeUnifiedConsentServiceClient::SetMetricsReportingEnabled(
    bool enabled) {
  // TODO(http://crbug.com/800974): Implement this method.
  NOTIMPLEMENTED();
}

void ChromeUnifiedConsentServiceClient::SetSearchSuggestEnabled(bool enabled) {
  // TODO(http://crbug.com/800974): Implement this method.
  NOTIMPLEMENTED();
}

void ChromeUnifiedConsentServiceClient::SetSafeBrowsingExtendedReportingEnabled(
    bool enabled) {
  // TODO(http://crbug.com/800974): Implement this method.
  NOTIMPLEMENTED();
}

void ChromeUnifiedConsentServiceClient::SetNetworkPredictionEnabled(
    bool enabled) {
  // TODO(http://crbug.com/800974): Implement this method.
  NOTIMPLEMENTED();
}
