// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_features_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcFeaturesParserTest : public testing::Test {
 public:
  ArcFeaturesParserTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcFeaturesParserTest);
};

constexpr const char kValidJson[] = R"json({"features": [
      {
        "name": "com.google.android.feature.GOOGLE_BUILD",
        "version": 0
      },
      {
        "name": "com.google.android.feature.GOOGLE_EXPERIENCE",
        "version": 2
      }
    ],
    "unavailable_features": [],
    "properties": {
      "ro.product.cpu.abilist": "x86_64,x86,armeabi-v7a,armeabi",
      "ro.build.version.sdk": "25"
    }})json";

constexpr const char kValidJsonWithUnavailableFeature[] =
    R"json({"features": [
      {
        "name": "android.software.home_screen",
        "version": 0
      },
      {
        "name": "com.google.android.feature.GOOGLE_EXPERIENCE",
        "version": 0
      }
    ],
    "unavailable_features": ["android.software.location"],
    "properties": {}})json";

constexpr const char kValidJsonFeatureEmptyName[] =
    R"json({"features": [
      {
        "name": "android.hardware.faketouch",
        "version": 0
      },
      {
        "name": "android.hardware.location",
        "version": 0
      },
      {
        "name": "",
        "version": 0
      }
    ],
    "unavailable_features": ["android.software.home_screen", ""],
    "properties": {}})json";

constexpr const char kValidJsonWithMissingFields[] =
    R"json({"invalid_root": [
      {
        "name": "android.hardware.location"
      },
      {
        "name": "android.hardware.location.network"
      }
    ],
    "invalid_root_second": [],
    "invalid_root_third": {}})json";

TEST_F(ArcFeaturesParserTest, ParseEmptyJson) {
  base::Optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(base::StringPiece());
  EXPECT_EQ(arc_features, base::nullopt);
}

TEST_F(ArcFeaturesParserTest, ParseInvalidJson) {
  base::Optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(
          kValidJsonWithMissingFields);
  EXPECT_EQ(arc_features, base::nullopt);
}

TEST_F(ArcFeaturesParserTest, ParseValidJson) {
  base::Optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(kValidJson);
  auto feature_map = arc_features->feature_map;
  auto unavailable_features = arc_features->unavailable_features;
  auto build_props = arc_features->build_props;
  EXPECT_EQ(feature_map.size(), 2u);
  EXPECT_EQ(unavailable_features.size(), 0u);
  EXPECT_EQ(build_props.size(), 2u);
}

TEST_F(ArcFeaturesParserTest, ParseValidJsonWithUnavailableFeature) {
  base::Optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(
          kValidJsonWithUnavailableFeature);
  auto feature_map = arc_features->feature_map;
  auto unavailable_features = arc_features->unavailable_features;
  auto build_props = arc_features->build_props;
  EXPECT_EQ(feature_map.size(), 2u);
  EXPECT_EQ(unavailable_features.size(), 1u);
  EXPECT_EQ(build_props.size(), 0u);
}

TEST_F(ArcFeaturesParserTest, ParseValidJsonWithEmptyFeatureName) {
  base::Optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(
          kValidJsonFeatureEmptyName);
  EXPECT_EQ(arc_features, base::nullopt);
}

}  // namespace
}  // namespace arc
