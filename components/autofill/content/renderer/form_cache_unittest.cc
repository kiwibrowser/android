// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class FormCacheTest : public testing::Test {
 public:
  FormCacheTest() {}

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FormCacheTest);
};

TEST_F(FormCacheTest, ShouldShowAutocompleteConsoleWarnings_Enabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillShowAutocompleteConsoleWarnings);
  FormCache form_cache(nullptr);

  // If we have a prediction and the actual attribute is empty.
  EXPECT_TRUE(
      form_cache.ShouldShowAutocompleteConsoleWarnings("given-name", ""));
  // There is a predicted type, and there is an recognized type that is not the
  // same.
  EXPECT_TRUE(
      form_cache.ShouldShowAutocompleteConsoleWarnings("given-name", "name"));
  // Multi-values: there is a predicted type, and there is an recognized type
  // that is not the same.
  EXPECT_TRUE(form_cache.ShouldShowAutocompleteConsoleWarnings(
      "given-name", "shipping name"));
  // Multi-values: there is a predicted type, and there is an recognized type
  // that is not the same along with an unrecognized type.
  EXPECT_TRUE(form_cache.ShouldShowAutocompleteConsoleWarnings("given-name",
                                                               "fake name"));

  // No predicted type, no actual attribute.
  EXPECT_FALSE(form_cache.ShouldShowAutocompleteConsoleWarnings("", ""));
  // No predicted type, and there is a recognized type.
  EXPECT_FALSE(
      form_cache.ShouldShowAutocompleteConsoleWarnings("", "given-name"));
  // No predicted type, and there is an unrecognized type.
  EXPECT_FALSE(form_cache.ShouldShowAutocompleteConsoleWarnings("", "fake"));
  // There is a predicted type, and there is an unrecognized type.
  EXPECT_FALSE(
      form_cache.ShouldShowAutocompleteConsoleWarnings("given-name", "fake"));
  // Multi-values: there is a predicted type, and there is an recognized type
  // that is the same.
  EXPECT_FALSE(form_cache.ShouldShowAutocompleteConsoleWarnings(
      "given-name", "shipping given-name"));
  // Multi-values: there is a predicted type, and there is an unrecognized type.
  EXPECT_FALSE(form_cache.ShouldShowAutocompleteConsoleWarnings(
      "given-name", "shipping fake"));
}

TEST_F(FormCacheTest, ShouldShowAutocompleteConsoleWarnings_Disabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillShowAutocompleteConsoleWarnings);
  FormCache form_cache(nullptr);

  // If we have a prediction and the actual attribute is empty.
  EXPECT_FALSE(
      form_cache.ShouldShowAutocompleteConsoleWarnings("given-name", ""));
  // There is a predicted type, and there is an recognized type that is not the
  // same.
  EXPECT_FALSE(
      form_cache.ShouldShowAutocompleteConsoleWarnings("given-name", "name"));
  // Multi-values: there is a predicted type, and there is an recognized type
  // that is not the same.
  EXPECT_FALSE(form_cache.ShouldShowAutocompleteConsoleWarnings(
      "given-name", "shipping name"));
}

}  // namespace autofill
