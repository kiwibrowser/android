// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

// blue
static const Color kFallbackTestColor = Color(0, 0, 255);

// black
static const Color kMainBgTestColor = Color(0, 0, 0);

// red
static const Color kTestColor = Color(255, 0, 0);

}  // namespace

class CSSVariableResolverTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();

    RuntimeEnabledFeatures::SetCSSEnvironmentVariablesEnabled(true);
    GetStyleEngine().EnsureEnvironmentVariables().SetVariable("test", "red");
  }

  void SetTestHTML(const String& value) {
    GetDocument().body()->SetInnerHTMLFromString(
        "<style>"
        "  #target {"
        "    --main-bg-color: black;"
        "    --test: red;"
        "    background-color: " +
        value +
        "  }"
        "</style>"
        "<div>"
        "  <div id=target></div>"
        "</div>");
    GetDocument().View()->UpdateAllLifecyclePhases();
  }
};

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Missing_NestedVar) {
  SetTestHTML("env(missing, var(--main-bg-color))");

  // Check that the element has the background color provided by the
  // nested variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kMainBgTestColor, target->ComputedStyleRef().VisitedDependentColor(
                                  GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Missing_NestedVar_Fallback) {
  SetTestHTML("env(missing, var(--missing, blue))");

  // Check that the element has the fallback background color.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kFallbackTestColor,
            target->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Missing_WithFallback) {
  SetTestHTML("env(missing, blue)");

  // Check that the element has the fallback background color.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kFallbackTestColor,
            target->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Valid) {
  SetTestHTML("env(test)");

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColor, target->ComputedStyleRef().VisitedDependentColor(
                            GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_Valid_WithFallback) {
  SetTestHTML("env(test, blue)");

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColor, target->ComputedStyleRef().VisitedDependentColor(
                            GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_WhenNested) {
  SetTestHTML("var(--main-bg-color, env(missing))");

  // Check that the element has the background color provided by var().
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kMainBgTestColor, target->ComputedStyleRef().VisitedDependentColor(
                                  GetCSSPropertyBackgroundColor()));
}

TEST_F(CSSVariableResolverTest, ParseEnvVariable_WhenNested_WillFallback) {
  SetTestHTML("var(--missing, env(test))");

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColor, target->ComputedStyleRef().VisitedDependentColor(
                            GetCSSPropertyBackgroundColor()));
}

}  // namespace blink
