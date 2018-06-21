// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DESKTOP_PROMOTION_DESKTOP_PROMOTION_SYNC_SERVICE_H
#define CHROME_BROWSER_DESKTOP_PROMOTION_DESKTOP_PROMOTION_SYNC_SERVICE_H

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class is responsible for observing the SyncService. Once the
// priority preferences are synced, it will check the desktop promotion
// pref and if eligible it will log the desktop promotion metrics to
// uma and mark the promotion cycle as completed in a pref.
class DesktopPromotionSyncService : public KeyedService,
                                    public syncer::SyncServiceObserver {
 public:
  // Only the DesktopPromotionSyncServiceFactory and tests should call this.
  DesktopPromotionSyncService(PrefService* pref_service,
                              syncer::SyncService* sync_service);

  ~DesktopPromotionSyncService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // Register profile specific desktop promotion related preferences.
  static void RegisterDesktopPromotionUserPrefs(
      user_prefs::PrefRegistrySyncable* registry);

 private:
  PrefService* pref_service_ = nullptr;
  syncer::SyncService* sync_service_ = nullptr;
  bool desktop_metrics_logger_initiated_ = false;

  DISALLOW_COPY_AND_ASSIGN(DesktopPromotionSyncService);
};

#endif  // CHROME_BROWSER_DESKTOP_PROMOTION_DESKTOP_PROMOTION_SYNC_SERVICE_H
