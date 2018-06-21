// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ERROR_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ERROR_H_

#include <string>
#include <utility>

#include "base/optional.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"

namespace content {

struct SignedExchangeError {
 public:
  enum class Field {
    kSignatureSig,
    kSignatureIintegrity,
    kSignatureCertUrl,
    kSignatureCertSha256,
    kSignatureValidityUrl,
    kSignatureTimestamps,
  };

  // |signature_index| will be used when we will support multiple signatures in
  // a signed exchange header to indicate which signature is causing the error.
  using FieldIndexPair = std::pair<int /* signature_index */, Field>;

  static base::Optional<Field> GetFieldFromSignatureVerifierResult(
      SignedExchangeSignatureVerifier::Result verify_result);

  SignedExchangeError(const std::string& message,
                      base::Optional<FieldIndexPair> field);

  // Copy constructor.
  SignedExchangeError(const SignedExchangeError& other);
  // Move constructor.
  SignedExchangeError(SignedExchangeError&& other);

  ~SignedExchangeError();

  std::string message;
  base::Optional<FieldIndexPair> field;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ERROR_H_
