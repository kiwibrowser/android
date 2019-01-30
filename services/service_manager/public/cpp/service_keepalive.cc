// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_keepalive.h"

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace service_manager {

ServiceKeepalive::ServiceKeepalive(ServiceContext* context,
                                   base::TimeDelta idle_timeout)
    : context_(context),
      idle_timeout_(idle_timeout),
      ref_factory_(base::BindRepeating(&ServiceKeepalive::OnRefCountZero,
                                       base::Unretained(this))),
      weak_ptr_factory_(this) {
  ref_factory_.SetRefAddedCallback(base::BindRepeating(
      &ServiceKeepalive::OnRefAdded, base::Unretained(this)));
}

ServiceKeepalive::~ServiceKeepalive() = default;

std::unique_ptr<ServiceContextRef> ServiceKeepalive::CreateRef() {
  return ref_factory_.CreateRef();
}

void ServiceKeepalive::OnRefAdded() {
  idle_timer_.Stop();
}

void ServiceKeepalive::OnRefCountZero() {
  idle_timer_.Start(FROM_HERE, idle_timeout_, context_->CreateQuitClosure());
}

}  // namespace service_manager
