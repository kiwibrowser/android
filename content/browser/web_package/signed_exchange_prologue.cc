// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prologue.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/signed_exchange_utils.h"

namespace content {

namespace {

constexpr char kSignedExchangeMagic[] = "sxg1-b1";
constexpr size_t kMaximumSignatureHeaderFieldLength = 16 * 1024;
constexpr size_t kMaximumCBORHeaderLength = 16 * 1024;

}  // namespace

constexpr size_t SignedExchangePrologue::kEncodedLengthInBytes;
size_t SignedExchangePrologue::kEncodedPrologueInBytes =
    sizeof(kSignedExchangeMagic) +
    SignedExchangePrologue::kEncodedLengthInBytes * 2;

// static
size_t SignedExchangePrologue::ParseEncodedLength(
    base::span<const uint8_t> input) {
  DCHECK_EQ(input.size(), SignedExchangePrologue::kEncodedLengthInBytes);
  return static_cast<size_t>(input[0]) << 16 |
         static_cast<size_t>(input[1]) << 8 | static_cast<size_t>(input[2]);
}

// static
base::Optional<SignedExchangePrologue> SignedExchangePrologue::Parse(
    base::span<const uint8_t> input,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangePrologue::Parse");

  CHECK_EQ(input.size(), kEncodedPrologueInBytes);

  const auto magic_string = input.subspan(0, sizeof(kSignedExchangeMagic));
  const auto encoded_signature_header_field_length =
      input.subspan(sizeof(kSignedExchangeMagic), kEncodedLengthInBytes);
  const auto encoded_cbor_header_length =
      input.subspan(sizeof(kSignedExchangeMagic) + kEncodedLengthInBytes,
                    kEncodedLengthInBytes);

  if (memcmp(magic_string.data(), kSignedExchangeMagic,
             sizeof(kSignedExchangeMagic)) != 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "Wrong magic string");
    return base::nullopt;
  }

  size_t signature_header_field_length =
      ParseEncodedLength(encoded_signature_header_field_length);
  size_t cbor_header_length = ParseEncodedLength(encoded_cbor_header_length);

  if (signature_header_field_length > kMaximumSignatureHeaderFieldLength) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Signature header field too long: %zu",
                           signature_header_field_length));
    return base::nullopt;
  }
  if (cbor_header_length > kMaximumCBORHeaderLength) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("CBOR header too long: %zu", cbor_header_length));
    return base::nullopt;
  }

  return SignedExchangePrologue(signature_header_field_length,
                                cbor_header_length);
}

size_t SignedExchangePrologue::ComputeFollowingSectionsLength() const {
  return signature_header_field_length_ + cbor_header_length_;
}

}  // namespace content
