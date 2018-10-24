// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_test_helper.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace {
const char kExtensionFeaturesHistogramName[] =
    "Blink.UseCounter.Extensions.Features";

const char kSVGFeaturesHistogramName[] = "Blink.UseCounter.SVGImage.Features";

// In practice, SVGs always appear to be loaded with an about:blank URL
const char kSvgUrl[] = "about:blank";
const char kExtensionUrl[] = "chrome-extension://dummysite/";

int GetPageVisitsBucketforHistogram(const std::string& histogram_name) {
  if (histogram_name.find("CSS") == std::string::npos)
    return static_cast<int>(blink::mojom::WebFeature::kPageVisits);
  // For CSS histograms, the page visits bucket should be 1.
  return blink::mojom::blink::kTotalPagesMeasuredCSSSampleId;
}

}  // namespace

namespace blink {
using WebFeature = mojom::WebFeature;

class UseCounterTest : public testing::Test {
 public:
  UseCounterTest() : dummy_(DummyPageHolder::Create()) {
    Page::InsertOrdinaryPageForTesting(&dummy_->GetPage());
  }

 protected:
  LocalFrame* GetFrame() { return &dummy_->GetFrame(); }
  void SetIsViewSource() { dummy_->GetDocument().SetIsViewSource(true); }
  void SetURL(const KURL& url) { dummy_->GetDocument().SetURL(url); }
  Document& GetDocument() { return dummy_->GetDocument(); }

  template <typename T>
  void HistogramBasicTest(const std::string& histogram,
                          T item,
                          T second_item,
                          std::function<bool(T)> counted,
                          std::function<void(T)> count,
                          std::function<int(T)> histogram_map,
                          std::function<void(LocalFrame*)> did_commit_load,
                          const std::string& url);
  std::unique_ptr<DummyPageHolder> dummy_;
  HistogramTester histogram_tester_;
};

template <typename T>
void UseCounterTest::HistogramBasicTest(
    const std::string& histogram,
    T item,
    T second_item,
    std::function<bool(T)> counted,
    std::function<void(T)> count,
    std::function<int(T)> histogram_map,
    std::function<void(LocalFrame*)> did_commit_load,
    const std::string& url) {
  int page_visits_bucket = GetPageVisitsBucketforHistogram(histogram);

  // Test recording a single (arbitrary) counter
  EXPECT_FALSE(counted(item));
  count(item);
  EXPECT_TRUE(counted(item));
  histogram_tester_.ExpectUniqueSample(histogram, histogram_map(item), 1);
  // Test that repeated measurements have no effect
  count(item);
  histogram_tester_.ExpectUniqueSample(histogram, histogram_map(item), 1);

  // Test recording a different sample
  EXPECT_FALSE(counted(second_item));
  count(second_item);
  EXPECT_TRUE(counted(second_item));
  histogram_tester_.ExpectBucketCount(histogram, histogram_map(item), 1);
  histogram_tester_.ExpectBucketCount(histogram, histogram_map(second_item), 1);
  histogram_tester_.ExpectTotalCount(histogram, 2);

  // After a page load, the histograms will be updated, even when the URL
  // scheme is internal
  SetURL(URLTestHelpers::ToKURL(url));
  did_commit_load(GetFrame());
  histogram_tester_.ExpectBucketCount(histogram, histogram_map(item), 1);
  histogram_tester_.ExpectBucketCount(histogram, histogram_map(second_item), 1);
  histogram_tester_.ExpectBucketCount(histogram, page_visits_bucket, 1);
  histogram_tester_.ExpectTotalCount(histogram, 3);

  // Now a repeat measurement should get recorded again, exactly once
  EXPECT_FALSE(counted(item));
  count(item);
  count(item);
  EXPECT_TRUE(counted(item));
  histogram_tester_.ExpectBucketCount(histogram, histogram_map(item), 2);
  histogram_tester_.ExpectTotalCount(histogram, 4);
}

TEST_F(UseCounterTest, RecordingExtensions) {
  UseCounter use_counter(UseCounter::kExtensionContext);
  HistogramBasicTest<WebFeature>(
      kExtensionFeaturesHistogramName, WebFeature::kFetch,
      WebFeature::kFetchBodyStream,
      [&](WebFeature feature) -> bool {
        return use_counter.HasRecordedMeasurement(feature);
      },
      [&](WebFeature feature) {
        use_counter.RecordMeasurement(feature, *GetFrame());
      },
      [](WebFeature feature) -> int { return static_cast<int>(feature); },
      [&](LocalFrame* frame) { use_counter.DidCommitLoad(frame); },
      kExtensionUrl);
}

TEST_F(UseCounterTest, SVGImageContextFeatures) {
  UseCounter use_counter(UseCounter::kSVGImageContext);
  HistogramBasicTest<WebFeature>(
      kSVGFeaturesHistogramName, WebFeature::kSVGSMILAdditiveAnimation,
      WebFeature::kSVGSMILAnimationElementTiming,
      [&](WebFeature feature) -> bool {
        return use_counter.HasRecordedMeasurement(feature);
      },
      [&](WebFeature feature) {
        use_counter.RecordMeasurement(feature, *GetFrame());
      },
      [](WebFeature feature) -> int { return static_cast<int>(feature); },
      [&](LocalFrame* frame) { use_counter.DidCommitLoad(frame); }, kSvgUrl);
}

TEST_F(UseCounterTest, CSSSelectorPseudoIS) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoIS;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>.a+:is(.b, .c+.d) { color: red; }</style>");
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

/*
 * Counter-specific tests
 *
 * NOTE: Most individual UseCounters don't need dedicated test cases.  They are
 * "tested" by analyzing the data they generate including on some known pages.
 * Feel free to add tests for counters where the triggering logic is
 * non-trivial, but it's not required. Manual analysis is necessary to trust the
 * data anyway, real-world pages are full of edge-cases and surprises that you
 * won't find in unit testing anyway.
 */

TEST_F(UseCounterTest, CSSSelectorPseudoAnyLink) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoAnyLink;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>:any-link { color: red; }</style>");
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST_F(UseCounterTest, CSSSelectorPseudoWebkitAnyLink) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoWebkitAnyLink;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>:-webkit-any-link { color: red; }</style>");
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST_F(UseCounterTest, CSSTypedOMStylePropertyMap) {
  UseCounter use_counter;
  WebFeature feature = WebFeature::kCSSTypedOMStylePropertyMap;
  EXPECT_FALSE(use_counter.IsCounted(GetDocument(), feature));
  use_counter.Count(GetDocument(), feature);
  EXPECT_TRUE(use_counter.IsCounted(GetDocument(), feature));
}

TEST_F(UseCounterTest, CSSSelectorPseudoMatches) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoMatches;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>.a+:matches(.b, .c+.d) { color: red; }</style>");
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST_F(UseCounterTest, CSSContainLayoutNonPositionedDescendants) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='contain: layout;'>"
      "</div>");
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
}

TEST_F(UseCounterTest, CSSContainLayoutAbsolutelyPositionedDescendants) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='contain: layout;'>"
      "  <div style='position: absolute;'></div>"
      "</div>");
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST_F(UseCounterTest,
       CSSContainLayoutAbsolutelyPositionedDescendantsAlreadyContainingBlock) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='position: relative; contain: layout;'>"
      "  <div style='position: absolute;'></div>"
      "</div>");
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
}

TEST_F(UseCounterTest, CSSContainLayoutFixedPositionedDescendants) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='contain: layout;'>"
      "  <div style='position: fixed;'></div>"
      "</div>");
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(UseCounter::IsCounted(document, feature));
}

TEST_F(UseCounterTest,
       CSSContainLayoutFixedPositionedDescendantsAlreadyContainingBlock) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='transform: translateX(100px); contain: layout;'>"
      "  <div style='position: fixed;'></div>"
      "</div>");
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(UseCounter::IsCounted(document, feature));
}

class DeprecationTest : public testing::Test {
 public:
  DeprecationTest()
      : dummy_(DummyPageHolder::Create()),
        deprecation_(dummy_->GetPage().GetDeprecation()),
        use_counter_(dummy_->GetPage().GetUseCounter()) {
    Page::InsertOrdinaryPageForTesting(&dummy_->GetPage());
  }

 protected:
  LocalFrame* GetFrame() { return &dummy_->GetFrame(); }

  std::unique_ptr<DummyPageHolder> dummy_;
  Deprecation& deprecation_;
  UseCounter& use_counter_;
};

TEST_F(DeprecationTest, InspectorDisablesDeprecation) {
  // The specific feature we use here isn't important.
  WebFeature feature = WebFeature::kCSSDeepCombinator;
  CSSPropertyID property = CSSPropertyFontWeight;

  EXPECT_FALSE(deprecation_.IsSuppressed(property));

  deprecation_.MuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.MuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  // TODO: use the actually deprecated property to get a deprecation message.
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(GetFrame(), feature);
  EXPECT_TRUE(use_counter_.HasRecordedMeasurement(feature));
}

}  // namespace blink
