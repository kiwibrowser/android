// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_device.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

namespace {

const char kActiveExclusiveSession[] =
    "XRDevice already has an active, exclusive session";

const char kExclusiveNotSupported[] =
    "XRDevice does not support the creation of exclusive sessions.";

const char kNoOutputContext[] =
    "Non-exclusive sessions must be created with an outputContext.";

const char kRequestRequiresUserActivation[] =
    "The requested session requires user activation.";

const char kSessionNotSupported[] =
    "The specified session configuration is not supported.";

const char kRequestFailed[] = "Request for XRSession failed.";

}  // namespace

XRDevice::XRDevice(
    XR* xr,
    device::mojom::blink::VRMagicWindowProviderPtr magic_window_provider,
    device::mojom::blink::VRDisplayHostPtr display,
    device::mojom::blink::VRDisplayClientRequest client_request,
    device::mojom::blink::VRDisplayInfoPtr display_info)
    : xr_(xr),
      magic_window_provider_(std::move(magic_window_provider)),
      display_(std::move(display)),
      display_client_binding_(this, std::move(client_request)) {
  SetXRDisplayInfo(std::move(display_info));
}

ExecutionContext* XRDevice::GetExecutionContext() const {
  return xr_->GetExecutionContext();
}

const AtomicString& XRDevice::InterfaceName() const {
  return EventTargetNames::XRDevice;
}

const char* XRDevice::checkSessionSupport(
    const XRSessionCreationOptions& options) const {
  if (!options.exclusive()) {
    // Validation for non-exclusive sessions. Validation for exclusive sessions
    // happens browser side.
    if (!options.hasOutputContext()) {
      return kNoOutputContext;
    }
  }

  // TODO(https://crbug.com/828321): Use session options instead of the flag.
  bool is_ar = RuntimeEnabledFeatures::WebXRHitTestEnabled();
  if (is_ar) {
    if (!supports_ar_) {
      return kSessionNotSupported;
    }
    // TODO(https://crbug.com/828321): Expose information necessary to check
    // combinations.
    // For now, exclusive AR is not supported.
    if (options.exclusive()) {
      return kSessionNotSupported;
    }
  } else {
    // TODO(https://crbug.com/828321): Remove this check when properly
    // supporting multiple VRDevice registration.
    if (supports_ar_) {
      // We don't expect to get an AR-capable device, but it can happen in
      // layout tests, due to mojo mocking. For now just reject the session
      // request.
      return kSessionNotSupported;
    }
    // TODO(https://crbug.com/828321): Check that VR is supported.
  }

  return nullptr;
}

ScriptPromise XRDevice::supportsSession(
    ScriptState* script_state,
    const XRSessionCreationOptions& options) {
  // Check to see if the device is capable of supporting the requested session
  // options. Note that reporting support here does not guarantee that creating
  // a session with those options will succeed, as other external and
  // time-sensitve factors (focus state, existance of another exclusive session,
  // etc.) may prevent the creation of a session as well.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           reject_reason));
  }

  // If the above checks pass, resolve without a value. Future API iterations
  // may specify a value to be returned here.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->exclusive = options.exclusive();

  display_->SupportsSession(
      std::move(session_options),
      WTF::Bind(&XRDevice::OnSupportsSessionReturned, WrapPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

void XRDevice::OnSupportsSessionReturned(ScriptPromiseResolver* resolver,
                                         bool supports_session) {
  // kExclusiveNotSupported is currently the only reason that SupportsSession
  // rejects on the browser side. That or there are no devices, but that should
  // technically not be possible.
  supports_session
      ? resolver->Resolve()
      : resolver->Reject(DOMException::Create(
            DOMExceptionCode::kNotSupportedError, kExclusiveNotSupported));
}

int64_t XRDevice::GetSourceId() const {
  return xr_->GetSourceId();
}

ScriptPromise XRDevice::requestSession(
    ScriptState* script_state,
    const XRSessionCreationOptions& options) {
  Document* doc = ToDocumentOrNull(ExecutionContext::From(script_state));

  if (options.exclusive() && !did_log_request_exclusive_session_ && doc) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_exclusive_session_ = true;
  }

  // Check first to see if the device is capable of supporting the requested
  // options.
  const char* reject_reason = checkSessionSupport(options);
  if (reject_reason) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           reject_reason));
  }

  // TODO(ijamardo): Should we just exit if there is not document?
  bool has_user_activation =
      Frame::HasTransientUserActivation(doc ? doc->GetFrame() : nullptr);

  // Check if the current page state prevents the requested session from being
  // created.
  if (options.exclusive()) {
    if (frameProvider()->exclusive_session()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kActiveExclusiveSession));
    }

    if (!has_user_activation) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kRequestRequiresUserActivation));
    }
  }

  // All AR sessions require a user gesture.
  // TODO(https://crbug.com/828321): Use session options instead.
  if (RuntimeEnabledFeatures::WebXRHitTestEnabled()) {
    if (!has_user_activation) {
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kRequestRequiresUserActivation));
    }
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  device::mojom::blink::XRSessionOptionsPtr session_options =
      device::mojom::blink::XRSessionOptions::New();
  session_options->exclusive = options.exclusive();
  session_options->has_user_activation = has_user_activation;

  // TODO(offenwanger): Once device activation is sorted out for WebXR, either
  // pass in the value for metrics, or remove the value as soon as legacy API
  // has been removed.
  display_->RequestSession(
      std::move(session_options), false /* triggered by display activate */,
      WTF::Bind(&XRDevice::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(resolver), options));
  return promise;
}

void XRDevice::OnRequestSessionReturned(
    ScriptPromiseResolver* resolver,
    const XRSessionCreationOptions& options,
    device::mojom::blink::XRPresentationConnectionPtr connection) {
  if (!connection) {
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kNotAllowedError, kRequestFailed);
    resolver->Reject(exception);
    return;
  }

  XRPresentationContext* output_context = nullptr;
  if (options.hasOutputContext()) {
    output_context = options.outputContext();
  }

  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  // TODO(https://crbug.com/828321): Use session options instead of the flag.
  if (RuntimeEnabledFeatures::WebXRHitTestEnabled()) {
    blend_mode = XRSession::kBlendModeAlphaBlend;
  }

  XRSession* session =
      new XRSession(this, options.exclusive(), output_context, blend_mode);
  sessions_.insert(session);

  if (options.exclusive()) {
    frameProvider()->BeginExclusiveSession(session, std::move(connection));
  }

  resolver->Resolve(session);
}

void XRDevice::OnFrameFocusChanged() {
  OnFocusChanged();
}

void XRDevice::OnFocusChanged() {
  // Tell all sessions that focus changed.
  for (const auto& session : sessions_) {
    session->OnFocusChanged();
  }

  if (frame_provider_)
    frame_provider_->OnFocusChanged();
}

bool XRDevice::IsFrameFocused() {
  return xr_->IsFrameFocused();
}

// TODO: Forward these calls on to the sessions once they've been implemented.
void XRDevice::OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) {
  SetXRDisplayInfo(std::move(display_info));
}
void XRDevice::OnExitPresent() {}
void XRDevice::OnBlur() {
  // The device is reporting to us that it is blurred.  This could happen for a
  // variety of reasons, such as browser UI, a different application using the
  // headset, or another page entering an exclusive session.
  has_device_focus_ = false;
  OnFocusChanged();
}
void XRDevice::OnFocus() {
  has_device_focus_ = true;
  OnFocusChanged();
}
void XRDevice::OnActivate(device::mojom::blink::VRDisplayEventReason,
                          OnActivateCallback on_handled) {}
void XRDevice::OnDeactivate(device::mojom::blink::VRDisplayEventReason) {}

XRFrameProvider* XRDevice::frameProvider() {
  if (!frame_provider_) {
    frame_provider_ = new XRFrameProvider(this);
  }

  return frame_provider_;
}

void XRDevice::Dispose() {
  display_client_binding_.Close();
  if (frame_provider_)
    frame_provider_->Dispose();
}

void XRDevice::SetXRDisplayInfo(
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  display_info_id_++;
  display_info_ = std::move(display_info);
  is_external_ = display_info_->capabilities->hasExternalDisplay;
  supports_exclusive_ = (display_info_->capabilities->canPresent);
  supports_ar_ = display_info_->capabilities->can_provide_pass_through_images;
}

void XRDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
