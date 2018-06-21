// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNIFIED_CONSENT_CHROME_UNIFIED_CONSENT_SERVICE_CLIENT_H_
#define CHROME_BROWSER_UNIFIED_CONSENT_CHROME_UNIFIED_CONSENT_SERVICE_CLIENT_H_

#include "base/macros.h"
#include "components/unified_consent/unified_consent_service_client.h"

class PrefService;

class ChromeUnifiedConsentServiceClient : public UnifiedConsentServiceClient {
 public:
  ChromeUnifiedConsentServiceClient(PrefService* pref_service);
  ~ChromeUnifiedConsentServiceClient() override = default;

  void SetAlternateErrorPagesEnabled(bool enabled) override;
  void SetMetricsReportingEnabled(bool enabled) override;
  void SetSearchSuggestEnabled(bool enabled) override;
  void SetSafeBrowsingExtendedReportingEnabled(bool enabled) override;
  void SetNetworkPredictionEnabled(bool enabled) override;

 private:
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeUnifiedConsentServiceClient);
};

#endif  // CHROME_BROWSER_UNIFIED_CONSENT_CHROME_UNIFIED_CONSENT_SERVICE_CLIENT_H_
