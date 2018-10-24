// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_

#include "ash/cast_config_controller.h"

namespace ash {

class CastNotificationController : public CastConfigControllerObserver {
 public:
  CastNotificationController();
  ~CastNotificationController() override;

  // CastConfigControllerObserver:
  void OnDevicesUpdated(std::vector<mojom::SinkAndRoutePtr> devices) override;

 private:
  void ShowNotification(std::vector<mojom::SinkAndRoutePtr> devices);
  void RemoveNotification();

  void StopCasting();

  // The cast activity id that we are displaying. If the user stops a cast, we
  // send this value to the config delegate so that we stop the right cast.
  mojom::CastRoutePtr displayed_route_;

  base::WeakPtrFactory<CastNotificationController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastNotificationController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
