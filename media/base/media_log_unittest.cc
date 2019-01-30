// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/macros.h"
#include "media/base/media_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Friend class of MediaLog for access to internal constants.
class MediaLogTest : public testing::Test {
 public:
  static constexpr size_t kMaxUrlLength = MediaLog::kMaxUrlLength;
};

constexpr size_t MediaLogTest::kMaxUrlLength;

TEST_F(MediaLogTest, DontTruncateShortUrlString) {
  const std::string short_url("chromium.org");
  EXPECT_LT(short_url.length(), MediaLogTest::kMaxUrlLength);

  // Verify that CreatedEvent does not truncate the short URL.
  std::unique_ptr<MediaLogEvent> created_event =
      MediaLog().CreateCreatedEvent(short_url);
  std::string stored_url;
  created_event->params.GetString("origin_url", &stored_url);
  EXPECT_EQ(stored_url, short_url);

  // Verify that LoadEvent does not truncate the short URL.
  std::unique_ptr<MediaLogEvent> load_event =
      MediaLog().CreateLoadEvent(short_url);
  load_event->params.GetString("url", &stored_url);
  EXPECT_EQ(stored_url, short_url);
}

TEST_F(MediaLogTest, TruncateLongUrlStrings) {
  // Build a long string that exceeds the URL length limit.
  std::stringstream string_builder;
  constexpr size_t kLongStringLength = MediaLogTest::kMaxUrlLength + 10;
  for (size_t i = 0; i < kLongStringLength; i++) {
    string_builder << "c";
  }
  const std::string long_url = string_builder.str();
  EXPECT_GT(long_url.length(), MediaLogTest::kMaxUrlLength);

  // Verify that long CreatedEvent URL...
  std::unique_ptr<MediaLogEvent> created_event =
      MediaLog().CreateCreatedEvent(long_url);
  std::string stored_url;
  created_event->params.GetString("origin_url", &stored_url);

  // ... is truncated
  EXPECT_EQ(stored_url.length(), MediaLogTest::kMaxUrlLength);
  // ... ends with ellipsis
  EXPECT_EQ(stored_url.compare(MediaLogTest::kMaxUrlLength - 3, 3, "..."), 0);
  // ... is otherwise a substring of the longer URL
  EXPECT_EQ(stored_url.compare(0, MediaLogTest::kMaxUrlLength - 3, long_url, 0,
                               MediaLogTest::kMaxUrlLength - 3),
            0);

  // Verify that long LoadEvent URL...
  std::unique_ptr<MediaLogEvent> load_event =
      MediaLog().CreateCreatedEvent(long_url);
  load_event->params.GetString("url", &stored_url);
  // ... is truncated
  EXPECT_EQ(stored_url.length(), MediaLogTest::kMaxUrlLength);
  // ... ends with ellipsis
  EXPECT_EQ(stored_url.compare(MediaLogTest::kMaxUrlLength - 3, 3, "..."), 0);
  // ... is otherwise a substring of the longer URL
  EXPECT_EQ(stored_url.compare(0, MediaLogTest::kMaxUrlLength - 3, long_url, 0,
                               MediaLogTest::kMaxUrlLength - 3),
            0);
}

}  // namespace media