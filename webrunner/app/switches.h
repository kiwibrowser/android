// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_APP_SWITCHES_H_
#define WEBRUNNER_APP_SWITCHES_H_

#include "webrunner/common/webrunner_export.h"

namespace fuchsia {

// "--context-process" signifies the process should be launched as a Context
// service.
// Omitting the switch signifies that the process should be a ContextProvider.
extern WEBRUNNER_EXPORT const char kContextProcess[];

}  // namespace fuchsia

#endif  // WEBRUNNER_APP_SWITCHES_H_
