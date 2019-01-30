// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fullscreen/element_fullscreen.h"

#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"

namespace blink {

ScriptPromise ElementFullscreen::requestFullscreen(
    ScriptState* script_state,
    Element& element,
    const FullscreenOptions& options) {
  return Fullscreen::RequestFullscreen(
      element, options, Fullscreen::RequestType::kUnprefixed, script_state);
}

void ElementFullscreen::webkitRequestFullscreen(Element& element) {
  webkitRequestFullscreen(element, FullscreenOptions());
}

void ElementFullscreen::webkitRequestFullscreen(
    Element& element,
    const FullscreenOptions& options) {
  if (element.IsInShadowTree()) {
    UseCounter::Count(element.GetDocument(),
                      WebFeature::kPrefixedElementRequestFullscreenInShadow);
  }
  Fullscreen::RequestFullscreen(element, options,
                                Fullscreen::RequestType::kPrefixed);
}

}  // namespace blink
