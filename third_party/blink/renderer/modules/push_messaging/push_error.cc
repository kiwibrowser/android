// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_error.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

DOMException* PushError::Take(ScriptPromiseResolver* resolver,
                              const WebPushError& web_error) {
  switch (web_error.error_type) {
    case WebPushError::kErrorTypeAbort:
      return DOMException::Create(DOMExceptionCode::kAbortError,
                                  web_error.message);
    case WebPushError::kErrorTypeInvalidState:
      return DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                  web_error.message);
    case WebPushError::kErrorTypeNetwork:
      return DOMException::Create(DOMExceptionCode::kNetworkError,
                                  web_error.message);
    case WebPushError::kErrorTypeNone:
      NOTREACHED();
      return DOMException::Create(DOMExceptionCode::kUnknownError,
                                  web_error.message);
    case WebPushError::kErrorTypeNotAllowed:
      return DOMException::Create(DOMExceptionCode::kNotAllowedError,
                                  web_error.message);
    case WebPushError::kErrorTypeNotFound:
      return DOMException::Create(DOMExceptionCode::kNotFoundError,
                                  web_error.message);
    case WebPushError::kErrorTypeNotSupported:
      return DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                  web_error.message);
  }
  NOTREACHED();
  return DOMException::Create(DOMExceptionCode::kUnknownError);
}

}  // namespace blink
