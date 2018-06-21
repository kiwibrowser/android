// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/cbor/cbor_writer.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "crypto/signature_verifier.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace content {

namespace {

// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#signature-validity
// Step 7. "Let message be the concatenation of the following byte strings."
constexpr uint8_t kMessageHeader[] =
    // 7.1. "A string that consists of octet 32 (0x20) repeated 64 times."
    // [spec text]
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    // 7.2. "A context string: the ASCII encoding of "HTTP Exchange 1"." ...
    // "but implementations of drafts MUST NOT use it and MUST use another
    // draft-specific string beginning with "HTTP Exchange 1 " instead."
    // [spec text]
    // 7.3. "A single 0 byte which serves as a separator." [spec text]
    "HTTP Exchange 1 b1";

base::Optional<cbor::CBORValue> GenerateCanonicalRequestCBOR(
    const SignedExchangeEnvelope& envelope) {
  cbor::CBORValue::MapValue map;
  map.insert_or_assign(
      cbor::CBORValue(kMethodKey, cbor::CBORValue::Type::BYTE_STRING),
      cbor::CBORValue(envelope.request_method(),
                      cbor::CBORValue::Type::BYTE_STRING));
  map.insert_or_assign(
      cbor::CBORValue(kUrlKey, cbor::CBORValue::Type::BYTE_STRING),
      cbor::CBORValue(envelope.request_url().spec(),
                      cbor::CBORValue::Type::BYTE_STRING));

  return cbor::CBORValue(map);
}

base::Optional<cbor::CBORValue> GenerateCanonicalResponseCBOR(
    const SignedExchangeEnvelope& envelope) {
  const auto& headers = envelope.response_headers();
  cbor::CBORValue::MapValue map;
  std::string response_code_str =
      base::NumberToString(envelope.response_code());
  map.insert_or_assign(
      cbor::CBORValue(kStatusKey, cbor::CBORValue::Type::BYTE_STRING),
      cbor::CBORValue(response_code_str, cbor::CBORValue::Type::BYTE_STRING));
  for (const auto& pair : headers) {
    map.insert_or_assign(
        cbor::CBORValue(pair.first, cbor::CBORValue::Type::BYTE_STRING),
        cbor::CBORValue(pair.second, cbor::CBORValue::Type::BYTE_STRING));
  }
  return cbor::CBORValue(map);
}

// Generate CBORValue from |envelope| as specified in:
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#cbor-representation
base::Optional<cbor::CBORValue> GenerateCanonicalExchangeHeadersCBOR(
    const SignedExchangeEnvelope& envelope) {
  auto req_val = GenerateCanonicalRequestCBOR(envelope);
  if (!req_val)
    return base::nullopt;
  auto res_val = GenerateCanonicalResponseCBOR(envelope);
  if (!res_val)
    return base::nullopt;

  cbor::CBORValue::ArrayValue array;
  array.push_back(std::move(*req_val));
  array.push_back(std::move(*res_val));
  return cbor::CBORValue(array);
}

// Generate a CBOR map value as specified in
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#signature-validity
// Step 7.4.
base::Optional<cbor::CBORValue> GenerateSignedMessageCBOR(
    const SignedExchangeEnvelope& envelope) {
  auto headers_val = GenerateCanonicalExchangeHeadersCBOR(envelope);
  if (!headers_val)
    return base::nullopt;

  // 7.4. "The bytes of the canonical CBOR serialization (Section 3.4) of
  // a CBOR map mapping:" [spec text]
  cbor::CBORValue::MapValue map;
  // 7.4.1. "If cert-sha256 is set: The text string "cert-sha256" to the byte
  // string value of cert-sha256." [spec text]
  if (envelope.signature().cert_sha256.has_value()) {
    map.insert_or_assign(
        cbor::CBORValue(kCertSha256Key),
        cbor::CBORValue(
            base::StringPiece(reinterpret_cast<const char*>(
                                  envelope.signature().cert_sha256->data),
                              sizeof(envelope.signature().cert_sha256->data)),
            cbor::CBORValue::Type::BYTE_STRING));
  }
  // 7.4.2. "The text string "validity-url" to the byte string value of
  // validity-url." [spec text]
  map.insert_or_assign(cbor::CBORValue(kValidityUrlKey),
                       cbor::CBORValue(envelope.signature().validity_url.spec(),
                                       cbor::CBORValue::Type::BYTE_STRING));
  // 7.4.3. "The text string "date" to the integer value of date." [spec text]
  if (!base::IsValueInRangeForNumericType<int64_t>(envelope.signature().date))
    return base::nullopt;

  map.insert_or_assign(
      cbor::CBORValue(kDateKey),
      cbor::CBORValue(base::checked_cast<int64_t>(envelope.signature().date)));
  // 7.4.4. "The text string "expires" to the integer value of expires."
  // [spec text]
  if (!base::IsValueInRangeForNumericType<int64_t>(
          envelope.signature().expires))
    return base::nullopt;

  map.insert_or_assign(cbor::CBORValue(kExpiresKey),
                       cbor::CBORValue(base::checked_cast<int64_t>(
                           envelope.signature().expires)));
  // 7.4.5. "The text string "headers" to the CBOR representation
  // (Section 3.2) of exchange's headers." [spec text]
  map.insert_or_assign(cbor::CBORValue(kHeadersKey), std::move(*headers_val));
  return cbor::CBORValue(map);
}

base::Optional<crypto::SignatureVerifier::SignatureAlgorithm>
GetSignatureAlgorithm(scoped_refptr<net::X509Certificate> cert,
                      SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "VerifySignature");
  base::StringPiece spki;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
          &spki)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "Failed to extract SPKI.");
    return base::nullopt;
  }

  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse public key.");
    return base::nullopt;
  }

  int pkey_id = EVP_PKEY_id(pkey.get());
  if (pkey_id != EVP_PKEY_EC) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Unsupported public key type: %d. Only ECDSA keys "
                           "on the secp256r1 curve are supported.",
                           pkey_id));
    return base::nullopt;
  }

  const EC_GROUP* group = EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(pkey.get()));
  int curve_name = EC_GROUP_get_curve_name(group);
  if (curve_name == NID_X9_62_prime256v1)
    return crypto::SignatureVerifier::ECDSA_SHA256;
  signed_exchange_utils::ReportErrorAndTraceEvent(
      devtools_proxy,
      base::StringPrintf("Unsupported EC group: %d. Only ECDSA keys on the "
                         "secp256r1 curve are supported.",
                         curve_name));
  return base::nullopt;
}

bool VerifySignature(base::span<const uint8_t> sig,
                     base::span<const uint8_t> msg,
                     scoped_refptr<net::X509Certificate> cert,
                     crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                     SignedExchangeDevToolsProxy* devtools_proxy) {
  crypto::SignatureVerifier verifier;
  if (!net::x509_util::SignatureVerifierInitWithCertificate(
          &verifier, algorithm, sig, cert->cert_buffer())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "SignatureVerifierInitWithCertificate failed.");
    return false;
  }
  verifier.VerifyUpdate(msg);
  if (!verifier.VerifyFinal()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "VerifyFinal failed.");
    return false;
  }
  return true;
}

std::string HexDump(const std::vector<uint8_t>& msg) {
  std::string output;
  for (const auto& byte : msg) {
    base::StringAppendF(&output, "%02x", byte);
  }
  return output;
}

base::Optional<std::vector<uint8_t>> GenerateSignedMessage(
    const SignedExchangeEnvelope& envelope) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "GenerateSignedMessage");

  // GenerateSignedMessageCBOR corresponds to Step 7.4.
  base::Optional<cbor::CBORValue> cbor_val =
      GenerateSignedMessageCBOR(envelope);
  if (!cbor_val) {
    TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "GenerateSignedMessage", "error",
                     "GenerateSignedMessageCBOR failed.");
    return base::nullopt;
  }

  base::Optional<std::vector<uint8_t>> cbor_message =
      cbor::CBORWriter::Write(*cbor_val);
  if (!cbor_message) {
    TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "GenerateSignedMessage", "error",
                     "CBORWriter::Write failed.");
    return base::nullopt;
  }

  // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#signature-validity
  // Step 7. "Let message be the concatenation of the following byte strings."
  std::vector<uint8_t> message;
  // see kMessageHeader for Steps 7.1 to 7.3.
  message.reserve(arraysize(kMessageHeader) + cbor_message->size());
  message.insert(message.end(), std::begin(kMessageHeader),
                 std::end(kMessageHeader));
  // 7.4. "The bytes of the canonical CBOR serialization (Section 3.4) of
  // a CBOR map mapping:" [spec text]
  message.insert(message.end(), cbor_message->begin(), cbor_message->end());
  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "GenerateSignedMessage", "dump", HexDump(message));
  return message;
}

base::Time TimeFromSignedExchangeUnixTime(uint64_t t) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(t);
}

// Implements steps 5-6 of
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#signature-validity
bool VerifyTimestamps(const SignedExchangeEnvelope& envelope,
                      const base::Time& verification_time) {
  base::Time expires_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().expires);
  base::Time creation_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().date);

  // 5. "If expires is more than 7 days (604800 seconds) after date, return
  // "invalid"." [spec text]
  if ((expires_time - creation_time).InSeconds() > 604800)
    return false;

  // 6. "If the current time is before date or after expires, return
  // "invalid"."
  if (verification_time < creation_time || expires_time < verification_time)
    return false;

  return true;
}

}  // namespace

SignedExchangeSignatureVerifier::Result SignedExchangeSignatureVerifier::Verify(
    const SignedExchangeEnvelope& envelope,
    scoped_refptr<net::X509Certificate> certificate,
    const base::Time& verification_time,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeSignatureVerifier::Verify");

  if (!VerifyTimestamps(envelope, verification_time)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Invalid timestamp. creation_time: %" PRIu64
            ", expires_time: %" PRIu64 ", verification_time: %" PRIu64,
            envelope.signature().date, envelope.signature().expires,
            (verification_time - base::Time::UnixEpoch()).InSeconds()));
    return Result::kErrInvalidTimestamp;
  }

  if (!certificate) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "No certificate set.");
    return Result::kErrNoCertificate;
  }

  if (!envelope.signature().cert_sha256.has_value()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "No cert-sha256 set.");
    return Result::kErrNoCertificateSHA256;
  }

  // The main-certificate is the first certificate in certificate-chain.
  if (*envelope.signature().cert_sha256 !=
      net::X509Certificate::CalculateFingerprint256(
          certificate->cert_buffer())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "cert-sha256 mismatch.");
    return Result::kErrCertificateSHA256Mismatch;
  }

  auto message = GenerateSignedMessage(envelope);
  if (!message) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to reconstruct signed message.");
    return Result::kErrInvalidSignatureFormat;
  }

  base::Optional<crypto::SignatureVerifier::SignatureAlgorithm> algorithm =
      GetSignatureAlgorithm(certificate, devtools_proxy);
  if (!algorithm)
    return Result::kErrUnsupportedCertType;

  const std::string& sig = envelope.signature().sig;
  if (!VerifySignature(
          base::make_span(reinterpret_cast<const uint8_t*>(sig.data()),
                          sig.size()),
          *message, certificate, *algorithm, devtools_proxy)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to verify signature \"sig\".");
    return Result::kErrSignatureVerificationFailed;
  }

  if (!base::EqualsCaseInsensitiveASCII(envelope.signature().integrity, "mi")) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        "The current implemention only supports \"mi\" integrity scheme.");
    return Result::kErrInvalidSignatureIntegrity;
  }
  return Result::kSuccess;
}

base::Optional<std::vector<uint8_t>>
SignedExchangeSignatureVerifier::EncodeCanonicalExchangeHeaders(
    const SignedExchangeEnvelope& envelope) {
  base::Optional<cbor::CBORValue> cbor_val =
      GenerateCanonicalExchangeHeadersCBOR(envelope);
  if (!cbor_val)
    return base::nullopt;

  return cbor::CBORWriter::Write(*cbor_val);
}

}  // namespace content
