// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_CLIENT_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_CLIENT_H_

class UnifiedConsentServiceClient {
 public:
  virtual ~UnifiedConsentServiceClient() {}

  // Enables/disables Link Doctor error pages.
  virtual void SetAlternateErrorPagesEnabled(bool enabled) = 0;
  // Enables/disables metrics reporting.
  virtual void SetMetricsReportingEnabled(bool enabled) = 0;
  // Enables/disables search suggestions.
  virtual void SetSearchSuggestEnabled(bool enabled) = 0;
  // Enables/disables extended safe browsing.
  virtual void SetSafeBrowsingExtendedReportingEnabled(bool enabled) = 0;
  // Enables/disables prediction of network actions.
  virtual void SetNetworkPredictionEnabled(bool enabled) = 0;
};

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_CLIENT_H_
