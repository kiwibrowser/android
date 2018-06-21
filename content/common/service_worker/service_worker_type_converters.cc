// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_type_converters.h"

#include "base/logging.h"

namespace mojo {

content::ServiceWorkerStatusCode
TypeConverter<content::ServiceWorkerStatusCode,
              blink::mojom::ServiceWorkerEventStatus>::
    Convert(blink::mojom::ServiceWorkerEventStatus status) {
  switch (status) {
    case blink::mojom::ServiceWorkerEventStatus::COMPLETED:
      return content::SERVICE_WORKER_OK;
    case blink::mojom::ServiceWorkerEventStatus::REJECTED:
      return content::SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;
    case blink::mojom::ServiceWorkerEventStatus::ABORTED:
      return content::SERVICE_WORKER_ERROR_ABORT;
  }
  NOTREACHED() << status;
  return content::SERVICE_WORKER_ERROR_FAILED;
}

}  // namespace mojo
