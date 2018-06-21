// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PROLOGUE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PROLOGUE_H_

#include <string>

#include "base/containers/span.h"
#include "base/optional.h"
#include "content/common/content_export.h"

namespace content {

class SignedExchangeDevToolsProxy;

// SignedExchangePrologue maps to the first bytes of
// the "application/signed-exchange" format.
// SignedExchangePrologue derives the lengths of the variable-length sections
// following the prologue bytes.
class CONTENT_EXPORT SignedExchangePrologue {
 public:
  // TODO(kouhei): Below should be made private (only friend from unittest)
  // after the b1 migration.
  static constexpr size_t kEncodedLengthInBytes = 3;
  // Parse encoded length of the variable-length field in the signed exchange.
  // Note: |input| must be pointing to a valid memory address that has at least
  // |kEncodedLengthInBytes|.
  static size_t ParseEncodedLength(base::span<const uint8_t> input);

  // Size of the prologue bytes of the "application/signed-exchange" format
  // which maps to this class.
  static size_t kEncodedPrologueInBytes;

  // Parses the first bytes of the "application/signed-exchange" format.
  // |input| must be a valid span with length of kEncodedPrologueInBytes.
  // If success, returns the result. Otherwise, returns nullopt and
  // reports the error to |devtools_proxy|.
  static base::Optional<SignedExchangePrologue> Parse(
      base::span<const uint8_t> input,
      SignedExchangeDevToolsProxy* devtools_proxy);

  SignedExchangePrologue(size_t signature_header_field_length,
                         size_t cbor_header_length)
      : signature_header_field_length_(signature_header_field_length),
        cbor_header_length_(cbor_header_length) {}
  SignedExchangePrologue(const SignedExchangePrologue&) = default;
  ~SignedExchangePrologue() = default;

  size_t signature_header_field_length() const {
    return signature_header_field_length_;
  }
  size_t cbor_header_length() const { return cbor_header_length_; }

  size_t ComputeFollowingSectionsLength() const;

 private:
  // Corresponds to `sigLength` in the spec text.
  // Encoded length of the Signature header field's value.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#application-signed-exchange
  size_t signature_header_field_length_;

  // Corresponds to `headerLength` in the spec text.
  // Length of the CBOR representation of the request and response headers.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#application-signed-exchange
  size_t cbor_header_length_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PROLOGUE_H_
