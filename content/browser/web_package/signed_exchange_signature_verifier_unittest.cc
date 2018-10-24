// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

#include "base/callback.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(SignedExchangeSignatureVerifier, EncodeCanonicalExchangeHeaders) {
  SignedExchangeEnvelope envelope;
  envelope.set_request_method("GET");
  envelope.set_request_url(GURL("https://example.com/index.html"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256");

  base::Optional<std::vector<uint8_t>> encoded =
      SignedExchangeSignatureVerifier::EncodeCanonicalExchangeHeaders(envelope);
  ASSERT_TRUE(encoded.has_value());

  static const uint8_t kExpected[] = {
      // clang-format off
      0x82, // array(2)
        0xa2, // map(2)
          0x44, 0x3a, 0x75, 0x72, 0x6c, // bytes ":url"
          0x58, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x65,
          0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f,
          0x69, 0x6e, 0x64, 0x65, 0x78, 0x2e, 0x68, 0x74, 0x6d, 0x6c,
          // bytes "https://example.com/index.html"

          0x47, 0x3a, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64, // bytes ":method"
          0x43, 0x47, 0x45, 0x54, // bytes "GET"

        0xa3, // map(3)
          0x47, 0x3a, 0x73,0x74, 0x61, 0x74, 0x75, 0x73, // bytes ":status"
          0x43, 0x32, 0x30, 0x30, // bytes "200"

          0x4c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x74, 0x79,
          0x70, 0x65, // bytes "content-type"
          0x58, 0x18, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x68, 0x74, 0x6d, 0x6c,
          0x3b, 0x20, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x75,
          0x74, 0x66, 0x2d, 0x38, // bytes "text/html; charset=utf-8"

          0x50, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e,
          0x63, 0x6f, 0x64, 0x69, 0x6e, 0x67, // bytes "content-encoding"
          0x49, 0x6d, 0x69, 0x2d, 0x73, 0x68, 0x61, 0x32, 0x35, 0x36,
          // bytes "mi-sha256"
      // clang-format on
  };
  EXPECT_THAT(*encoded,
              testing::ElementsAreArray(kExpected, arraysize(kExpected)));
}

const uint64_t kSignatureHeaderDate = 1517892341;
const uint64_t kSignatureHeaderExpires = 1517895941;

// See content/testdata/htxg/README on how to generate this data.
// clang-format off
constexpr char kSignatureHeaderRSA[] =
    "label; "
    "sig=*RBFZPtl5xPDQyZuq4TcXY9fPkso5Edl7NofpdA9Bylwhvdsd7uCBAmOYx0BvXjrg8UVj"
    "axIHeVNavLzTU42NZgSBd3po1qrT4TZb6piN/BMqmBWtaxEFxLaLZyBgrQpXN/l+OkWSvCF30"
    "J9QEhqaI749SlVrrV37121Ik/WBIuo6Peo88HRP9292FEsrgwH3ggTJcTvkBbOIttO3UddEtN"
    "3hQNNowNhsUCr3fXn0lIMW8Gyp0V6TVedIhgT7zqUxRqJRjedQzY+Bm7F01/jKzvD1etAcw7r"
    "CidWFISmcyWjsLG1dlNtiZynO9gyyZduOSzBwEb9QcMTHekFsnmzFtg==*; "
    "validity-url=\"https://example.com/resource.validity.msg\"; "
    "integrity=\"mi\"; "
    "cert-url=\"https://example.com/cert.msg\"; "
    "cert-sha256=*tJGJP8ej7KCEW8VnVK3bKwpBza/oLrtWA75z5ZPptuc=*; "
    "date=1517892341; expires=1517895941";
// clang-format on

// See content/testdata/htxg/README on how to generate this data.
// clang-format off
constexpr char kSignatureHeaderECDSAP256[] =
    "label; "
    "sig=*MEUCIEbg974hkbM6gy0bT4ZpO0afUtpeViz+mojLqtSnqepvAiEApKfMyaKxhE8xofyW"
    "DlBjGTwsoOvNBycL9YfN9C72Rhs=*; "
    "validity-url=\"https://example.com/resource.validity.msg\"; "
    "integrity=\"mi\"; "
    "cert-url=\"https://example.com/cert.msg\"; "
    "cert-sha256=*CfDj40tr5B7oo6IaWwQF2L1uDgsHH0fA2YOCB7E0tAQ=*; "
    "date=1517892341; expires=1517895941";
// clang-format on

// See content/testdata/htxg/README on how to generate this data.
// clang-format off
constexpr char kSignatureHeaderECDSAP384[] =
    "label; "
    "sig=*MGUCMQC2Sw+qw8pFB8S7gCqFdJlaimbZCA9BOOnjPHuRa8nGbYwnQBJEZnNwWxW+7ffwQ"
    "skCMBVWAWya/ahn1XebSGAFeV8d6jC/xe9Rc8YCvb/KlV0tRxF0v06VasWcHx6OL8gZUg==*; "
    "validity-url=\"https://example.com/resource.validity.msg\"; "
    "integrity=\"mi\"; "
    "cert-url=\"https://example.com/cert.msg\"; "
    "cert-sha256=*8X8y8nj8vDJHSSa0cxn+TCu+8zGpIJfbdzAnd5cW+jA=*; "
    "date=1517892341; expires=1517895941";
// clang-format on

// |expires| (1518497142) is more than 7 days (604800 seconds) after |date|
// (1517892341).
// clang-format off
constexpr char kSignatureHeaderInvalidExpires[] =
    "sig; "
    "sig=*RhjjWuXi87riQUu90taBHFJgTo8XBhiCe9qTJMP7/XVPu2diRGipo06HoGsyXkidHiiW"
    "743JgoNmO7CjfeVXLXQgKDxtGidATtPsVadAT4JpBDZJWSUg5qAbWcASXjyO38Uhq9gJkeu4w"
    "1MRMGkvpgVXNjYhi5/9NUer1xEUuJh5UbIDhGrfMihwj+c30nW+qz0n5lCrYonk+Sc0jGcLgc"
    "aDLptqRhOG5S+avwKmbQoqtD0JSc/53L5xXjppyvSA2fRmoDlqVQpX4uzRKq9cny7fZ3qgpZ/"
    "YOCuT7wMj7oVEur175QLe2F8ktKH9arSEiquhFJxBIIIXza8PJnmL5w==*;"
    "validity-url=\"https://example.com/resource.validity.msg\"; "
    "integrity=\"mi\"; "
    "cert-url=\"https://example.com/cert.msg\"; "
    "cert-sha256=*3wfzkF4oKGUwoQ0rE7U11FIdcA/8biGzlaACeRQQH6k=*; "
    "date=1517892341; expires=1518497142";
// clang-format on

constexpr char kCertPEMRSA[] = R"(
-----BEGIN CERTIFICATE-----
MIIDyTCCArGgAwIBAgIBBDANBgkqhkiG9w0BAQsFADBjMQswCQYDVQQGEwJVUzET
MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEQMA4G
A1UECgwHVGVzdCBDQTEVMBMGA1UEAwwMVGVzdCBSb290IENBMB4XDTE3MDYwNTE3
MTA0NloXDTI3MDYwMzE3MTA0NlowYDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNh
bGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3Qg
Q0ExEjAQBgNVBAMMCTEyNy4wLjAuMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC
AQoCggEBANOUHzO0uxUyd3rYUArq33olXC0N1AYNM0wFTjUqUrElLiX48+5hERkG
hGwC8VG5Zr/2Jw/wtarLiDjg2OfPdwyMp3S7MBTgvXWZ989MUHpx6b0cWM298iOg
/VeinMphFLDfPDHFWZ7RXBqfk6MGLhI5GgvoooYw2jUmP+elnoizIL/OB08sIYra
AVrwasoRd+yOmyvQnzw3mZNKpWjeX7NhZCg2nG8B8u78agwAYVWupHnJS2GwhLzy
19AxU/HmaI9kyyMGmRtbRZ0roCyMDOgEEcWUSYNRP33KLi31uKYqOSblvzmC7kA7
k5yca3VXlgqg4gnjr9tbOMzMcpeqeaMCAwEAAaOBijCBhzAMBgNVHRMBAf8EAjAA
MB0GA1UdDgQWBBQYDOtRudM2qckEr/kvFPCZZtJ21DAfBgNVHSMEGDAWgBSbJguK
mKm7HbkfHOMaQDPtjheIqzAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIw
GAYDVR0RBBEwD4INKi5leGFtcGxlLm9yZzANBgkqhkiG9w0BAQsFAAOCAQEAvXK0
UF19i7JkSSdomQwB18WRFaKG8VZpSFsKbEECPRHoxktMl/Pd04wk+W0fZFq433j3
4D+cjTB6OxAVdPIPSex8U40fYMl9C13K1tejf4o/+rcLxEDdVfv7PUkogrliXzSE
MCYdcTwruV7hjC2/Ib0t/kdxblRt4dD2I1jdntsFy/VfET/m0J2qRhJWlfYEzCFe
Hn8H/PZIiIsso5pm2RodTqi9w4/+1r8Yyfmk8TF+EoWDYtbZ+ScgtCH5fldS+onI
hHgjz/tniqjbY0MRFr9ZxrohmtgOBOvROEKH06c92oOmj2ahyFpM/yU9PL/JvNmF
SaMW1eOzjHemIWKTMw==
-----END CERTIFICATE-----)";

constexpr char kCertPEMECDSAP256[] = R"(
-----BEGIN CERTIFICATE-----
MIICQzCCASsCAQEwDQYJKoZIhvcNAQELBQAwYzELMAkGA1UEBhMCVVMxEzARBgNV
BAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoM
B1Rlc3QgQ0ExFTATBgNVBAMMDFRlc3QgUm9vdCBDQTAeFw0xODAzMjMwNDU3MzRa
Fw0xOTAzMTgwNDU3MzRaMDcxGTAXBgNVBAMMEHRlc3QuZXhhbXBsZS5vcmcxDTAL
BgNVBAoMBFRlc3QxCzAJBgNVBAYTAlVTMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcD
QgAECQYn3HDPPhtMv2hzyjI7E3FU89EjnzTtvLd9OP55GLAsaE/FCTWbx6rKOxF7
O4jP0N3PsIzr+nT1lIix+HpxujANBgkqhkiG9w0BAQsFAAOCAQEAhKdVMvKm7gBz
af6nfCkLGRo56KJasi6lJh2byF17vdqq+mSXR+jHZtsRsRZJyl+C+jaSzrT0TnMA
kLg+U4ZnKZD5sTo7TWnRlTA4G4tOrWaq1tn89FWqe+hbvn6dEyTZ1XFPaO6hzeNH
ZM5H+bIpngvGmP1lf7K6PtC3Tx/S938zBdQrfKz/4ZB0S5cmIyIUBnlj3PDWtLsB
KS4wvSnjPj1EyVKxTQH1PdB2NqC4eT8bgFcryNWrkMOWdOUNhGWB55nVwI8yNPQO
4OrKJLsDZir3v7dzcU9U1erBp4+udGFIfW86g24FX1gn3SavtO6lZt59AFLptyQ6
LWh1CMv1aQ==
-----END CERTIFICATE-----)";

constexpr char kCertPEMECDSAP384[] = R"(
-----BEGIN CERTIFICATE-----
MIICYDCCAUgCAQEwDQYJKoZIhvcNAQELBQAwYzELMAkGA1UEBhMCVVMxEzARBgNV
BAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoM
B1Rlc3QgQ0ExFTATBgNVBAMMDFRlc3QgUm9vdCBDQTAeFw0xODA0MDkwMTUyMzVa
Fw0xOTA0MDQwMTUyMzVaMDcxGTAXBgNVBAMMEHRlc3QuZXhhbXBsZS5vcmcxDTAL
BgNVBAoMBFRlc3QxCzAJBgNVBAYTAlVTMHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE
YK0FPc6B2UkDO3GHS95PLss9e82f8RdQDIZE9UPUSOJ1UISOT19j/SJq3gyoY+pK
J818LhVe+ywgdH+tKosO6v1l2o/EffIRDjCfN/aSUuQjkkSwgyL62/9687+486z6
MA0GCSqGSIb3DQEBCwUAA4IBAQB61Q+/68hsD5OapG+2CDsJI+oR91H+Jv+tRMby
of47O0hJGISuAB9xcFhIcMKwBReODpBmzwSO713NNU/oaG/XysHH1TNZZodTtWD9
Z1g5AJamfwvFS+ObqzOtyFUdFS4NBAE4lXi5XnHa2hU2Bkm+abVYLqyAGw1kh2ES
DGC2vA1lb2Uy9bgLCYYkZoESjb/JYRQjCmqlwYKOozU7ZbIe3zJPjRWYP1Tuany5
+rYllWk/DJlMVjs/fbf0jj32vrevCgul43iWMgprOw1ncuK8l5nND/o5aN2mwMDw
Xhe5DP7VATeQq3yGV3ps+rCTHDP6qSHDEWP7DqHQdSsxtI0E
-----END CERTIFICATE-----)";

class SignedExchangeSignatureVerifierTest : public ::testing::Test {
 protected:
  SignedExchangeSignatureVerifierTest() {}

  const base::Time VerificationTime() {
    return base::Time::UnixEpoch() +
           base::TimeDelta::FromSeconds(kSignatureHeaderDate);
  }

  void TestVerifierGivenValidInput(
      const SignedExchangeEnvelope& envelope,
      scoped_refptr<net::X509Certificate> certificate) {
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kSuccess,
              SignedExchangeSignatureVerifier::Verify(
                  envelope, certificate, VerificationTime(),
                  nullptr /* devtools_proxy */));

    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
              SignedExchangeSignatureVerifier::Verify(
                  envelope, certificate,
                  base::Time::UnixEpoch() +
                      base::TimeDelta::FromSeconds(kSignatureHeaderDate - 1),
                  nullptr /* devtools_proxy */
                  ));

    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kSuccess,
              SignedExchangeSignatureVerifier::Verify(
                  envelope, certificate,
                  base::Time::UnixEpoch() +
                      base::TimeDelta::FromSeconds(kSignatureHeaderExpires),
                  nullptr /* devtools_proxy */
                  ));

    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
              SignedExchangeSignatureVerifier::Verify(
                  envelope, certificate,
                  base::Time::UnixEpoch() +
                      base::TimeDelta::FromSeconds(kSignatureHeaderExpires + 1),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope invalid_expires_envelope(envelope);
    auto invalid_expires_signature =
        SignedExchangeSignatureHeaderField::ParseSignature(
            kSignatureHeaderInvalidExpires, nullptr /* devtools_proxy */);
    ASSERT_TRUE(invalid_expires_signature.has_value());
    ASSERT_EQ(1u, invalid_expires_signature->size());
    invalid_expires_envelope.SetSignatureForTesting(
        (*invalid_expires_signature)[0]);
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
              SignedExchangeSignatureVerifier::Verify(
                  invalid_expires_envelope, certificate, VerificationTime(),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope corrupted_envelope(envelope);
    corrupted_envelope.set_request_url(GURL("https://example.com/bad.html"));
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::
                  kErrSignatureVerificationFailed,
              SignedExchangeSignatureVerifier::Verify(
                  corrupted_envelope, certificate, VerificationTime(),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope badsig_envelope(envelope);
    SignedExchangeSignatureHeaderField::Signature badsig = envelope.signature();
    badsig.sig[0]++;
    badsig_envelope.SetSignatureForTesting(badsig);
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::
                  kErrSignatureVerificationFailed,
              SignedExchangeSignatureVerifier::Verify(
                  badsig_envelope, certificate, VerificationTime(),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope badsigsha256_envelope(envelope);
    SignedExchangeSignatureHeaderField::Signature badsigsha256 =
        envelope.signature();
    badsigsha256.cert_sha256->data[0]++;
    badsigsha256_envelope.SetSignatureForTesting(badsigsha256);
    EXPECT_EQ(
        SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch,
        SignedExchangeSignatureVerifier::Verify(badsigsha256_envelope,
                                                certificate, VerificationTime(),
                                                nullptr /* devtools_proxy */
                                                ));
  }
};

TEST_F(SignedExchangeSignatureVerifierTest, VerifyRSA) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderRSA, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  net::CertificateList certlist =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMRSA, base::size(kCertPEMRSA),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, certlist.size());

  SignedExchangeEnvelope envelope;
  envelope.set_request_method("GET");
  envelope.set_request_url(GURL("https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256");
  envelope.AddResponseHeader(
      "mi", "mi-sha256=wmp4dRMYgxP3tSMCwV_I0CWOCiHZpAihKZk19bsN9RI");
  envelope.SetSignatureForTesting((*signature)[0]);

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrUnsupportedCertType,
            SignedExchangeSignatureVerifier::Verify(
                envelope, certlist[0], VerificationTime(),
                nullptr /* devtools_proxy */));
}

TEST_F(SignedExchangeSignatureVerifierTest, VerifyECDSAP256) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderECDSAP256, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  net::CertificateList certlist =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMECDSAP256, base::size(kCertPEMECDSAP256),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, certlist.size());

  SignedExchangeEnvelope envelope;
  envelope.set_request_method("GET");
  envelope.set_request_url(GURL("https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256");
  envelope.AddResponseHeader(
      "mi", "mi-sha256=wmp4dRMYgxP3tSMCwV_I0CWOCiHZpAihKZk19bsN9RI");

  envelope.SetSignatureForTesting((*signature)[0]);

  TestVerifierGivenValidInput(envelope, certlist[0]);
}

TEST_F(SignedExchangeSignatureVerifierTest, VerifyECDSAP384) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderECDSAP384, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  net::CertificateList certlist =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMECDSAP384, base::size(kCertPEMECDSAP384),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, certlist.size());

  SignedExchangeEnvelope envelope;
  envelope.set_request_method("GET");
  envelope.set_request_url(GURL("https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256");
  envelope.AddResponseHeader(
      "mi", "mi-sha256=wmp4dRMYgxP3tSMCwV_I0CWOCiHZpAihKZk19bsN9RIG");

  envelope.SetSignatureForTesting((*signature)[0]);

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrUnsupportedCertType,
            SignedExchangeSignatureVerifier::Verify(
                envelope, certlist[0], VerificationTime(),
                nullptr /* devtools_proxy */));
}

}  // namespace
}  // namespace content
