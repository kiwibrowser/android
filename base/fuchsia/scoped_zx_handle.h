// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCOPED_ZX_HANDLE_H_
#define BASE_FUCHSIA_SCOPED_ZX_HANDLE_H_

#include <lib/zx/handle.h>
#include <zircon/types.h>

#include "base/base_export.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/scoped_generic.h"

namespace base {

// TODO(852541): Temporary shim to implement the old ScopedGeneric based
// container as a native zx::handle. Remove this once all callers have been
// migrated to use the libzx containers.
class BASE_EXPORT ScopedZxHandle : public zx::handle {
 public:
  ScopedZxHandle() = default;
  explicit ScopedZxHandle(zx_handle_t h) : zx::handle(h) {}

  // Helper to converts zx::channel to ScopedZxHandle.
  static ScopedZxHandle FromZxChannel(zx::channel channel);

  // Helper to adapt between the libzx and ScopedGeneric APIs for receiving
  // handles directly into the container via an out-parameter.
  zx_handle_t* receive() { return reset_and_get_address(); }
};

}  // namespace base

#endif  // BASE_FUCHSIA_SCOPED_ZX_HANDLE_H_
