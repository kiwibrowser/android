// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request_init.h"

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_abort_signal.h"
#include "third_party/blink/renderer/core/fetch/headers.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"

namespace blink {

struct RequestInit::IDLPassThrough
    : public IDLBaseHelper<v8::Local<v8::Value>> {};

template <>
struct NativeValueTraits<RequestInit::IDLPassThrough>
    : public NativeValueTraitsBase<RequestInit::IDLPassThrough> {
  static v8::Local<v8::Value> NativeValue(v8::Isolate* isolate,
                                          v8::Local<v8::Value> value,
                                          ExceptionState& exception_state) {
    DCHECK(!value.IsEmpty());
    return value;
  }
};

class RequestInit::GetterHelper {
  STACK_ALLOCATED();

 public:
  // |this| object must not outlive |src| and |exception_state|.
  GetterHelper(const Dictionary& src, ExceptionState& exception_state)
      : src_(src), exception_state_(exception_state) {}

  template <typename IDLType>
  base::Optional<typename IDLType::ImplType> Get(const StringView& key) {
    auto r = src_.Get<IDLType>(key, exception_state_);
    are_any_members_set_ = are_any_members_set_ || r.has_value();
    return r;
  }

  bool AreAnyMembersSet() const { return are_any_members_set_; }

 private:
  const Dictionary& src_;
  ExceptionState& exception_state_;
  bool are_any_members_set_ = false;
  DISALLOW_COPY_AND_ASSIGN(GetterHelper);
};

RequestInit::RequestInit(ScriptState* script_state,
                         const Dictionary& options,
                         ExceptionState& exception_state) {
  GetterHelper h(options, exception_state);
  ExecutionContext* context = ExecutionContext::From(script_state);

  method_ = h.Get<IDLByteString>("method").value_or(String());
  if (exception_state.HadException())
    return;

  auto v8_headers = h.Get<IDLPassThrough>("headers");
  if (exception_state.HadException())
    return;

  mode_ = h.Get<IDLUSVString>("mode").value_or(String());
  if (exception_state.HadException())
    return;

  cache_ = h.Get<IDLUSVString>("cache").value_or(String());
  if (exception_state.HadException())
    return;

  redirect_ = h.Get<IDLUSVString>("redirect").value_or(String());
  if (exception_state.HadException())
    return;

  auto referrer_string = h.Get<IDLUSVString>("referrer");
  if (exception_state.HadException())
    return;

  auto referrer_policy_string = h.Get<IDLUSVString>("referrerPolicy");
  if (exception_state.HadException())
    return;

  integrity_ = h.Get<IDLString>("integrity").value_or(String());
  if (exception_state.HadException())
    return;

  if (RuntimeEnabledFeatures::PriorityHintsEnabled()) {
    importance_ = h.Get<IDLString>("importance").value_or(String());
    if (exception_state.HadException())
      return;
  }

  keepalive_ = h.Get<IDLBoolean>("keepalive");
  if (exception_state.HadException())
    return;

  base::Optional<v8::Local<v8::Value>> v8_signal;
  // In order to distinguish between undefined and null, split the steps of
  // looking it up in the dictionary and converting to the native type.
  v8_signal = h.Get<IDLPassThrough>("signal");
  if (exception_state.HadException())
    return;

  auto v8_body = h.Get<IDLPassThrough>("body");
  if (exception_state.HadException())
    return;

  credentials_ = h.Get<IDLUSVString>("credentials").value_or(String());

  if (exception_state.HadException())
    return;

  are_any_members_set_ = h.AreAnyMembersSet();

  CheckEnumValues(referrer_string, referrer_policy_string, exception_state);
  if (exception_state.HadException())
    return;

  v8::Isolate* isolate = ToIsolate(context);

  if (v8_headers.has_value()) {
    V8ByteStringSequenceSequenceOrByteStringByteStringRecord::ToImpl(
        isolate, *v8_headers, headers_, UnionTypeConversionMode::kNotNullable,
        exception_state);
    if (exception_state.HadException())
      return;
  }

  if (v8_signal.has_value()) {
    if ((*v8_signal)->IsNull()) {
      // Override any existing value.
      signal_.emplace(nullptr);
    } else {
      signal_.emplace(NativeValueTraits<AbortSignal>::NativeValue(
          isolate, *v8_signal, exception_state));
    }
    if (exception_state.HadException())
      return;
  }

  if (v8_body.has_value())
    body_ = ScriptValue(script_state, *v8_body);
}

base::Optional<AbortSignal*> RequestInit::Signal() const {
  return signal_.has_value() ? base::make_optional(signal_.value().Get())
                             : base::nullopt;
}

void RequestInit::CheckEnumValues(
    const base::Optional<String>& referrer_string,
    const base::Optional<String>& referrer_policy_string,
    ExceptionState& exception_state) {
  TRACE_EVENT0("blink", "RequestInit::CheckEnumValues");

  // Validate cache_
  if (!cache_.IsNull() && cache_ != "default" && cache_ != "no-store" &&
      cache_ != "reload" && cache_ != "no-cache" && cache_ != "force-cache" &&
      cache_ != "only-if-cached") {
    exception_state.ThrowTypeError("Invalid cache mode");
    return;
  }

  // Validate credentials_
  if (!credentials_.IsNull() && credentials_ != "omit" &&
      credentials_ != "same-origin" && credentials_ != "include") {
    exception_state.ThrowTypeError("Invalid credentials mode");
    return;
  }

  // Validate mode_
  if (!mode_.IsNull() && mode_ != "navigate" && mode_ != "same-origin" &&
      mode_ != "no-cors" && mode_ != "cors") {
    exception_state.ThrowTypeError("Invalid mode");
    return;
  }

  // Validate redirect_
  if (!redirect_.IsNull() && redirect_ != "follow" && redirect_ != "error" &&
      redirect_ != "manual") {
    exception_state.ThrowTypeError("Invalid redirect mode");
    return;
  }

  // Validate importance_
  if (!importance_.IsNull() && importance_ != "low" && importance_ != "auto" &&
      importance_ != "high") {
    exception_state.ThrowTypeError("Invalid importance mode");
    return;
  }

  // Validate referrer policy

  // A part of the Request constructor algorithm is performed here. See
  // the comments in the Request constructor code for the detail.

  // We need to use "about:client" instead of |clientReferrerString|,
  // because "about:client" => |clientReferrerString| conversion is done
  // in Request::createRequestWithRequestOrString.
  referrer_ = Referrer("about:client", kReferrerPolicyDefault);
  if (referrer_string.has_value())
    referrer_.referrer = AtomicString(*referrer_string);

  if (referrer_policy_string.has_value()) {
    if (*referrer_policy_string == "") {
      referrer_.referrer_policy = kReferrerPolicyDefault;
    } else if (*referrer_policy_string == "no-referrer") {
      referrer_.referrer_policy = kReferrerPolicyNever;
    } else if (*referrer_policy_string == "no-referrer-when-downgrade") {
      referrer_.referrer_policy = kReferrerPolicyNoReferrerWhenDowngrade;
    } else if (*referrer_policy_string == "origin") {
      referrer_.referrer_policy = kReferrerPolicyOrigin;
    } else if (*referrer_policy_string == "origin-when-cross-origin") {
      referrer_.referrer_policy = kReferrerPolicyOriginWhenCrossOrigin;
    } else if (*referrer_policy_string == "same-origin") {
      referrer_.referrer_policy = kReferrerPolicySameOrigin;
    } else if (*referrer_policy_string == "strict-origin") {
      referrer_.referrer_policy = kReferrerPolicyStrictOrigin;
    } else if (*referrer_policy_string == "unsafe-url") {
      referrer_.referrer_policy = kReferrerPolicyAlways;
    } else if (*referrer_policy_string == "strict-origin-when-cross-origin") {
      referrer_.referrer_policy = kReferrerPolicyStrictOriginWhenCrossOrigin;
    } else {
      exception_state.ThrowTypeError("Invalid referrer policy");
      return;
    }
  }
}

}  // namespace blink
