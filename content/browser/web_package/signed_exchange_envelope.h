// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ENVELOPE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ENVELOPE_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "content/common/content_export.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace content {

class SignedExchangeDevToolsProxy;

// SignedExchangeEnvelope contains all information captured in
// the "application/signed-exchange" format but the payload.
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html
class CONTENT_EXPORT SignedExchangeEnvelope {
 public:
  using HeaderMap = std::map<std::string, std::string>;

  // Parse headers from the application/signed-exchange;v=b0 format.
  // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#application-signed-exchange
  //
  // This also performs the step 1, 3 and 4 of "Cross-origin trust" validation.
  // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#cross-origin-trust
  static base::Optional<SignedExchangeEnvelope> Parse(
      base::StringPiece signature_header_field,
      base::span<const uint8_t> cbor_header,
      SignedExchangeDevToolsProxy* devtools_proxy);
  SignedExchangeEnvelope();
  SignedExchangeEnvelope(const SignedExchangeEnvelope&);
  SignedExchangeEnvelope(SignedExchangeEnvelope&&);
  SignedExchangeEnvelope& operator=(SignedExchangeEnvelope&&);
  ~SignedExchangeEnvelope();

  // AddResponseHeader returns false on duplicated keys. |name| must be
  // lower-cased.
  bool AddResponseHeader(base::StringPiece name, base::StringPiece value);
  scoped_refptr<net::HttpResponseHeaders> BuildHttpResponseHeaders() const;

  const GURL& request_url() const { return request_url_; };
  void set_request_url(GURL url) { request_url_ = std::move(url); }

  const std::string& request_method() const { return request_method_; }
  void set_request_method(base::StringPiece s) {
    s.CopyToString(&request_method_);
  }

  net::HttpStatusCode response_code() const { return response_code_; }
  void set_response_code(net::HttpStatusCode c) { response_code_ = c; }

  const HeaderMap& response_headers() const { return response_headers_; }

  const SignedExchangeSignatureHeaderField::Signature& signature() const {
    return signature_;
  }
  void SetSignatureForTesting(
      const SignedExchangeSignatureHeaderField::Signature& sig) {
    signature_ = sig;
  }

 private:
  GURL request_url_;
  std::string request_method_;

  net::HttpStatusCode response_code_;
  HeaderMap response_headers_;
  SignedExchangeSignatureHeaderField::Signature signature_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ENVELOPE_H_
