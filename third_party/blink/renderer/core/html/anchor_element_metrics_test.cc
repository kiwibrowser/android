// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class AnchorElementMetricsTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 400;
  static constexpr int kViewportHeight = 600;

 protected:
  AnchorElementMetricsTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().Resize(WebSize(kViewportWidth, kViewportHeight));
  }
};

// The main frame contains an anchor element.
// Features of the element are extracted.
// Then the test scrolls down to check features again.
TEST_F(AnchorElementMetricsTest, AnchorFeatureExtract) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://example.com">example</a>
    <div style='height: 10000px;'></div>
    </body>)HTML",
      2 * kViewportHeight));

  Element* anchor = GetDocument().getElementById("anchor");
  HTMLAnchorElement* anchor_element = ToHTMLAnchorElement(anchor);
  DCHECK(anchor_element);

  auto feature = AnchorElementMetrics::From(*anchor_element).value();
  EXPECT_GT(feature.GetRatioArea(), 0);
  EXPECT_FLOAT_EQ(feature.GetRatioDistanceRootTop(), 2);
  EXPECT_FLOAT_EQ(feature.GetRatioDistanceVisibleTop(), 2);
  EXPECT_EQ(feature.GetIsInIframe(), false);

  // Scroll down.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight / 2), kProgrammaticScroll);

  auto feature2 = AnchorElementMetrics::From(*anchor_element).value();
  EXPECT_FLOAT_EQ(feature2.GetRatioDistanceRootTop(), 2);
  EXPECT_FLOAT_EQ(feature2.GetRatioDistanceVisibleTop(), 1.5);
}

// The main frame contains an iframe. The iframe contains an anchor element.
// Features of the element are extracted.
// Then the test scrolls down in the main frame to check features again.
// Then the test scrolls down in the iframe to check features again.
TEST_F(AnchorElementMetricsTest, AnchorFeatureInIframe) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body style='margin: 0px'>
        <div style='height: %dpx;'></div>
        <iframe id='iframe' src='https://example.com/iframe.html'
            style='width: 200px; height: 200px;
            border-style: none; padding: 0px; margin: 0px;'></iframe>
        <div style='height: 10000px;'></div>
        </body>)HTML",
      2 * kViewportHeight));

  iframe_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://example.com">example</a>
    <div style='height: 10000px;'></div>
    </body>)HTML",
      kViewportHeight / 2));

  Element* iframe = GetDocument().getElementById("iframe");
  HTMLIFrameElement* iframe_element = ToHTMLIFrameElement(iframe);
  Frame* sub = iframe_element->ContentFrame();
  DCHECK(sub && sub->IsLocalFrame());
  LocalFrame* subframe = ToLocalFrame(sub);

  Element* anchor = subframe->GetDocument()->getElementById("anchor");
  HTMLAnchorElement* anchor_element = ToHTMLAnchorElement(anchor);
  DCHECK(anchor_element);

  auto feature = AnchorElementMetrics::From(*anchor_element).value();
  EXPECT_GT(feature.GetRatioArea(), 0);
  EXPECT_FLOAT_EQ(feature.GetRatioDistanceRootTop(), 2.5);
  EXPECT_FLOAT_EQ(feature.GetRatioDistanceVisibleTop(), 2.5);
  EXPECT_EQ(feature.GetIsInIframe(), true);

  // Scroll down the main frame.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 1.5), kProgrammaticScroll);

  auto feature2 = AnchorElementMetrics::From(*anchor_element).value();
  EXPECT_FLOAT_EQ(feature2.GetRatioDistanceRootTop(), 2.5);
  EXPECT_FLOAT_EQ(feature2.GetRatioDistanceVisibleTop(), 1);

  // Scroll down inside iframe.
  subframe->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 0.2), kProgrammaticScroll);

  auto feature3 = AnchorElementMetrics::From(*anchor_element).value();
  EXPECT_FLOAT_EQ(feature3.GetRatioDistanceRootTop(), 2.5);
  EXPECT_FLOAT_EQ(feature3.GetRatioDistanceVisibleTop(), 0.8);
}

}  // namespace blink
