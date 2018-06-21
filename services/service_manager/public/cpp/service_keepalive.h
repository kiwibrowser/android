// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_

#include "services/service_manager/public/cpp/export.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace service_manager {

class ServiceContext;

// Helper class which vends ServiceContextRefs from its own
// ServiceContextRefFactory. Whenever the ref count goes to zero, this starts an
// idle timer (configured at construction time). If the timer runs out before
// another ref is created, this requests clean service termination from the
// service manager on the service's behalf.
//
// Useful if you want your service to stay alive for some fixed delay after
// going idle, to insulate against frequent startup and shutdown of the service
// when used at regular intervals or in rapid but not continuous succession, as
// is fairly common.
//
// Use this in place of directly owning a ServiceContextRefFactory, to vend
// service references to different endpoints in your service.
class SERVICE_MANAGER_PUBLIC_CPP_EXPORT ServiceKeepalive {
 public:
  // Creates a keepalive which allows the service to be idle for |idle_timeout|
  // before requesting termination. Must outlive |context|, which is not owned.
  ServiceKeepalive(ServiceContext* context, base::TimeDelta idle_timeout);
  ~ServiceKeepalive();

  std::unique_ptr<ServiceContextRef> CreateRef();

 private:
  void OnRefAdded();
  void OnRefCountZero();

  ServiceContext* const context_;
  const base::TimeDelta idle_timeout_;
  base::OneShotTimer idle_timer_;
  ServiceContextRefFactory ref_factory_;
  base::WeakPtrFactory<ServiceKeepalive> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceKeepalive);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_KEEPALIVE_H_
