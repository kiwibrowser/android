// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_environment_variables.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

static const char kTestHTML[] = R"HTML(
      <style>
        #target { background-color: env(test); }
      </style>
      <div>
        <div id=target></div>
      </div>
    )HTML";

static const char kVariableName[] = "test";

// red
static const Color kTestColorRed = Color(255, 0, 0);
static const char kVariableTestColor[] = "red";

// blue
static const Color kAltTestColor = Color(0, 0, 255);
static const char kVariableAltTestColor[] = "blue";

// no set
static const Color kNoColor = Color(0, 0, 0, 0);

}  // namespace

class StyleEnvironmentVariablesTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();

    RuntimeEnabledFeatures::SetCSSEnvironmentVariablesEnabled(true);
  }

  void TearDown() override {
    StyleEnvironmentVariables::GetRootInstance().ClearForTesting();
  }

  DocumentStyleEnvironmentVariables& GetDocumentVariables() {
    return GetStyleEngine().EnsureEnvironmentVariables();
  }

  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    // Sets the inner html and runs the document lifecycle.
    frame.GetDocument()->body()->SetInnerHTMLFromString(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhases();
  }

  void SimulateNavigation() {
    const KURL& url = KURL(NullURL(), "https://www.example.com");
    FrameLoadRequest request(nullptr, ResourceRequest(url),
                             SubstituteData(SharedBuffer::Create()));
    GetDocument().GetFrame()->Loader().CommitNavigation(request);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }
};

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_AfterLoad) {
  InitializeWithHTML(GetFrame(), kTestHTML);
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Change) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableAltTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Change the variable value after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest,
       DocumentVariable_Override_RemoveDocument) {
  // Set the variable globally.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the document variable.
  GetDocumentVariables().RemoveVariable(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the global
  // variable.
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Override_RemoveGlobal) {
  // Set the variable globally.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Check that the element has the background color provided by the global
  // variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kAltTestColor, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value on the document after we have loaded the page.
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the document
  // variable.
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Remove the global variable.
  StyleEnvironmentVariables::GetRootInstance().RemoveVariable(kVariableName);

  // Ensure that the document has not been invalidated.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Preset) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, DocumentVariable_Remove) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  GetDocumentVariables().RemoveVariable(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element does not have the background color any more.
  EXPECT_NE(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, MultiDocumentInvalidation_FromRoot) {
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Create a second page that uses the variable.
  std::unique_ptr<DummyPageHolder> new_page =
      DummyPageHolder::Create(IntSize(800, 600));
  InitializeWithHTML(new_page->GetFrame(), kTestHTML);

  // Create an empty page that does not use the variable.
  std::unique_ptr<DummyPageHolder> empty_page =
      DummyPageHolder::Create(IntSize(800, 600));
  empty_page->GetDocument().View()->UpdateAllLifecyclePhases();

  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // The first two pages should be invalidated and the empty one should not.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(new_page->GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(empty_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, MultiDocumentInvalidation_FromDocument) {
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Create a second page that uses the variable.
  std::unique_ptr<DummyPageHolder> new_page =
      DummyPageHolder::Create(IntSize(800, 600));
  InitializeWithHTML(new_page->GetFrame(), kTestHTML);

  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Only the first document should be invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(new_page->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEnvironmentVariablesTest, NavigateToClear) {
  GetDocumentVariables().SetVariable(kVariableName, kVariableTestColor);

  // Simulate a navigation to clear the variables.
  SimulateNavigation();
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Check that the element has no background color.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kNoColor, target->ComputedStyleRef().VisitedDependentColor(
                          GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_AfterLoad) {
  InitializeWithHTML(GetFrame(), kTestHTML);
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Change) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(
      kVariableName, kVariableAltTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Change the variable value after we have loaded the page.
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Preset) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleEnvironmentVariablesTest, GlobalVariable_Remove) {
  StyleEnvironmentVariables::GetRootInstance().SetVariable(kVariableName,
                                                           kVariableTestColor);
  InitializeWithHTML(GetFrame(), kTestHTML);

  // Check that the element has the background color provided by the variable.
  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));

  // Change the variable value after we have loaded the page.
  StyleEnvironmentVariables::GetRootInstance().RemoveVariable(kVariableName);

  // Ensure that the document has been invalidated.
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that the element does not have the background color any more.
  EXPECT_NE(kTestColorRed, target->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyBackgroundColor()));
}

}  // namespace blink
