// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prologue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(SignedExchangePrologueTest, ParseEncodedLength) {
  constexpr struct {
    uint8_t bytes[SignedExchangePrologue::kEncodedLengthInBytes];
    size_t expected;
  } kTestCases[] = {
      {{0x00, 0x00, 0x01}, 1u}, {{0x01, 0xe2, 0x40}, 123456u},
  };

  int test_element_index = 0;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "testing case " << test_element_index++);
    EXPECT_EQ(SignedExchangePrologue::ParseEncodedLength(test_case.bytes),
              test_case.expected);
  }
}

TEST(SignedExchangePrologueTest, Simple) {
  uint8_t bytes[] = {'s',  'x',  'g',  '1',  '-',  'b',  '1',
                     '\0', 0x00, 0x12, 0x34, 0x00, 0x23, 0x45};

  auto prologue = SignedExchangePrologue::Parse(base::make_span(bytes),
                                                nullptr /* devtools_proxy */);
  EXPECT_TRUE(prologue);
  EXPECT_EQ(0x1234u, prologue->signature_header_field_length());
  EXPECT_EQ(0x2345u, prologue->cbor_header_length());
  EXPECT_EQ(0x3579u, prologue->ComputeFollowingSectionsLength());
}

TEST(SignedExchangePrologueTest, WrongMagic) {
  uint8_t bytes[] = {'s',  'x',  'g',  '!',  '-',  'b',  '1',
                     '\0', 0x00, 0x12, 0x34, 0x00, 0x23, 0x45};

  EXPECT_FALSE(SignedExchangePrologue::Parse(base::make_span(bytes),
                                             nullptr /* devtools_proxy */));
}

TEST(SignedExchangePrologueTest, LongSignatureHeaderField) {
  uint8_t bytes[] = {'s',  'x',  'g',  '1',  '-',  'b',  '1',
                     '\0', 0xff, 0x12, 0x34, 0x00, 0x23, 0x45};

  EXPECT_FALSE(SignedExchangePrologue::Parse(base::make_span(bytes),
                                             nullptr /* devtools_proxy */));
}

TEST(SignedExchangePrologueTest, LongCBORHeader) {
  uint8_t bytes[] = {'s',  'x',  'g',  '1',  '-',  'b',  '1',
                     '\0', 0x00, 0x12, 0x34, 0xff, 0x23, 0x45};

  EXPECT_FALSE(SignedExchangePrologue::Parse(base::make_span(bytes),
                                             nullptr /* devtools_proxy */));
}

}  // namespace content
