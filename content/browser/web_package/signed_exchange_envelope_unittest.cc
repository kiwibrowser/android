// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_envelope.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/cbor/cbor_values.h"
#include "components/cbor/cbor_writer.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_prologue.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kSignatureString[] =
    "sig1;"
    " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
    "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
    " integrity=\"mi\";"
    " validity-url=\"https://test.example.org/resource.validity.1511128380\";"
    " cert-url=\"https://example.com/oldcerts\";"
    " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
    " date=1511128380; expires=1511733180";

cbor::CBORValue CBORByteString(const char* str) {
  return cbor::CBORValue(str, cbor::CBORValue::Type::BYTE_STRING);
}

base::Optional<SignedExchangeEnvelope> GenerateHeaderAndParse(
    base::StringPiece signature,
    const std::map<const char*, const char*>& request_map,
    const std::map<const char*, const char*>& response_map) {
  cbor::CBORValue::MapValue request_cbor_map;
  cbor::CBORValue::MapValue response_cbor_map;
  for (auto& pair : request_map)
    request_cbor_map[CBORByteString(pair.first)] = CBORByteString(pair.second);
  for (auto& pair : response_map)
    response_cbor_map[CBORByteString(pair.first)] = CBORByteString(pair.second);

  cbor::CBORValue::ArrayValue array;
  array.push_back(cbor::CBORValue(std::move(request_cbor_map)));
  array.push_back(cbor::CBORValue(std::move(response_cbor_map)));

  auto serialized = cbor::CBORWriter::Write(cbor::CBORValue(std::move(array)));
  return SignedExchangeEnvelope::Parse(
      signature, base::make_span(serialized->data(), serialized->size()),
      nullptr /* devtools_proxy */);
}

}  // namespace

TEST(SignedExchangeEnvelopeTest, ParseGoldenFile) {
  base::FilePath test_htxg_path;
  base::PathService::Get(content::DIR_TEST_DATA, &test_htxg_path);
  test_htxg_path = test_htxg_path.AppendASCII("htxg").AppendASCII(
      "test.example.org_test.htxg");

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_htxg_path, &contents));
  auto* contents_bytes = reinterpret_cast<const uint8_t*>(contents.data());

  ASSERT_GT(contents.size(), SignedExchangePrologue::kEncodedPrologueInBytes);
  base::Optional<SignedExchangePrologue> prologue =
      SignedExchangePrologue::Parse(
          base::make_span(contents_bytes,
                          SignedExchangePrologue::kEncodedPrologueInBytes),
          nullptr /* devtools_proxy */);
  ASSERT_TRUE(prologue.has_value());
  ASSERT_GT(contents.size(), SignedExchangePrologue::kEncodedPrologueInBytes +
                                 prologue->ComputeFollowingSectionsLength());

  base::StringPiece signature_header_field(
      contents.data() + SignedExchangePrologue::kEncodedPrologueInBytes,
      prologue->signature_header_field_length());
  const auto cbor_bytes = base::make_span<const uint8_t>(
      contents_bytes + SignedExchangePrologue::kEncodedPrologueInBytes +
          prologue->signature_header_field_length(),
      prologue->cbor_header_length());
  const base::Optional<SignedExchangeEnvelope> envelope =
      SignedExchangeEnvelope::Parse(signature_header_field, cbor_bytes,
                                    nullptr /* devtools_proxy */);
  ASSERT_TRUE(envelope.has_value());
  EXPECT_EQ(envelope->request_url(), GURL("https://test.example.org/test/"));
  EXPECT_EQ(envelope->request_method(), "GET");
  EXPECT_EQ(envelope->response_code(), static_cast<net::HttpStatusCode>(200u));
  EXPECT_EQ(envelope->response_headers().size(), 3u);
  EXPECT_EQ(envelope->response_headers().find("content-encoding")->second,
            "mi-sha256");
}

TEST(SignedExchangeEnvelopeTest, ValidHeader) {
  auto header = GenerateHeaderAndParse(
      kSignatureString,
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {{kStatusKey, "200"}, {"content-type", "text/html"}});
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->request_url(), GURL("https://test.example.org/test/"));
  EXPECT_EQ(header->request_method(), "GET");
  EXPECT_EQ(header->response_code(), static_cast<net::HttpStatusCode>(200u));
  EXPECT_EQ(header->response_headers().size(), 1u);
}

TEST(SignedExchangeEnvelopeTest, UnsafeMethod) {
  auto header = GenerateHeaderAndParse(
      kSignatureString,
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "POST"},
      },
      {
          {kStatusKey, "200"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, InvalidURL) {
  auto header = GenerateHeaderAndParse(
      kSignatureString,
      {
          {kUrlKey, "https:://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, URLWithFragment) {
  auto header = GenerateHeaderAndParse(
      kSignatureString,
      {
          {kUrlKey, "https://test.example.org/test/#foo"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, RelativeURL) {
  auto header =
      GenerateHeaderAndParse(kSignatureString,
                             {
                                 {kUrlKey, "test/"}, {kMethodKey, "GET"},
                             },
                             {
                                 {kStatusKey, "200"},
                             });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, StatefulRequestHeader) {
  auto header =
      GenerateHeaderAndParse(kSignatureString,
                             {
                                 {kUrlKey, "https://test.example.org/test/"},
                                 {kMethodKey, "GET"},
                                 {"authorization", "Basic Zm9vOmJhcg=="},
                             },
                             {
                                 {kStatusKey, "200"},
                             });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, StatefulResponseHeader) {
  auto header = GenerateHeaderAndParse(
      kSignatureString,
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"}, {"set-cookie", "foo=bar"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, UppercaseRequestMap) {
  auto header =
      GenerateHeaderAndParse(kSignatureString,
                             {{kUrlKey, "https://test.example.org/test/"},
                              {kMethodKey, "GET"},
                              {"Accept-Language", "en-us"}},
                             {
                                 {kStatusKey, "200"},
                             });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, UppercaseResponseMap) {
  auto header = GenerateHeaderAndParse(
      kSignatureString,
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {{kStatusKey, "200"}, {"Content-Length", "123"}});
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, InvalidValidityURLHeader) {
  auto header = GenerateHeaderAndParse(
      kSignatureString,
      {
          {kUrlKey, "https://test2.example.org/test/"}, {kMethodKey, "GET"},
      },
      {{kStatusKey, "200"}, {"content-type", "text/html"}});
  ASSERT_FALSE(header.has_value());
}

}  // namespace content
