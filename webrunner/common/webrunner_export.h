// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_COMMON_WEBRUNNER_EXPORT_H_
#define WEBRUNNER_COMMON_WEBRUNNER_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WEBRUNNER_IMPLEMENTATION)
#define WEBRUNNER_EXPORT __attribute__((visibility("default")))
#else
#define WEBRUNNER_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define WEBRUNNER_EXPORT
#endif

#endif  // WEBRUNNER_COMMON_WEBRUNNER_EXPORT_H_
