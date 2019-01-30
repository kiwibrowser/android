// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

namespace blink {

TEST_F(AccessibilityTest, IsDescendantOf) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(button->IsDescendantOf(*root));
  EXPECT_FALSE(root->IsDescendantOf(*root));
  EXPECT_FALSE(button->IsDescendantOf(*button));
  EXPECT_FALSE(root->IsDescendantOf(*button));
}

TEST_F(AccessibilityTest, IsAncestorOf) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(root->IsAncestorOf(*button));
  EXPECT_FALSE(root->IsAncestorOf(*root));
  EXPECT_FALSE(button->IsAncestorOf(*button));
  EXPECT_FALSE(button->IsAncestorOf(*root));
}

TEST_F(AccessibilityTest, SimpleTreeNavigation) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <p id="paragraph">hello<br id="br">there</p>
                   <button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_EQ(input, root->FirstChild());
  EXPECT_EQ(button, root->LastChild());
  EXPECT_EQ(button, root->DeepestLastChild());

  ASSERT_NE(nullptr, paragraph->FirstChild());
  EXPECT_EQ(AccessibilityRole::kStaticTextRole,
            paragraph->FirstChild()->RoleValue());
  ASSERT_NE(nullptr, paragraph->LastChild());
  EXPECT_EQ(AccessibilityRole::kStaticTextRole,
            paragraph->LastChild()->RoleValue());
  ASSERT_NE(nullptr, paragraph->DeepestFirstChild());
  EXPECT_EQ(AccessibilityRole::kStaticTextRole,
            paragraph->DeepestFirstChild()->RoleValue());
  ASSERT_NE(nullptr, paragraph->DeepestLastChild());
  EXPECT_EQ(AccessibilityRole::kStaticTextRole,
            paragraph->DeepestLastChild()->RoleValue());

  EXPECT_EQ(paragraph->PreviousSibling(), input);
  EXPECT_EQ(paragraph, input->NextSibling());
  ASSERT_NE(nullptr, br->NextSibling());
  EXPECT_EQ(AccessibilityRole::kStaticTextRole, br->NextSibling()->RoleValue());
  ASSERT_NE(nullptr, br->PreviousSibling());
  EXPECT_EQ(AccessibilityRole::kStaticTextRole,
            br->PreviousSibling()->RoleValue());
}

TEST_F(AccessibilityTest, AXObjectComparisonOperators) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <p id="paragraph">hello<br id="br">there</p>
                   <button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(*root == *root);
  EXPECT_FALSE(*root != *root);
  EXPECT_FALSE(*root < *root);
  EXPECT_TRUE(*root <= *root);
  EXPECT_FALSE(*root > *root);
  EXPECT_TRUE(*root >= *root);

  EXPECT_TRUE(*input > *root);
  EXPECT_TRUE(*input >= *root);
  EXPECT_FALSE(*input < *root);
  EXPECT_FALSE(*input <= *root);

  EXPECT_TRUE(*input != *root);
  EXPECT_TRUE(*input < *paragraph);
  EXPECT_TRUE(*br > *input);
  EXPECT_TRUE(*paragraph < *br);
  EXPECT_TRUE(*br >= *paragraph);

  EXPECT_TRUE(*paragraph < *button);
  EXPECT_TRUE(*button > *br);
  EXPECT_FALSE(*button < *button);
  EXPECT_TRUE(*button <= *button);
  EXPECT_TRUE(*button >= *button);
  EXPECT_FALSE(*button > *button);
}

TEST_F(AccessibilityTest, AXObjectAncestorsIterator) {
  SetBodyInnerHTML(
      R"HTML(<p id="paragraph"><b id="bold"><br id="br"></b></p>)HTML");

  AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  AXObject* bold = GetAXObjectByElementId("bold");
  ASSERT_NE(nullptr, bold);
  AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, br->RoleValue());

  AXObject::AncestorsIterator iter = br->AncestorsBegin();
  EXPECT_EQ(*paragraph, *iter);
  EXPECT_EQ(AccessibilityRole::kParagraphRole, iter->RoleValue());
  EXPECT_EQ(*root, *++iter);
  EXPECT_EQ(*root, *iter++);
  EXPECT_EQ(br->AncestorsEnd(), ++iter);
}

TEST_F(AccessibilityTest, AXObjectInOrderTraversalIterator) {
  SetBodyInnerHTML(R"HTML(<button id="button">Button</button>)HTML");

  AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  AXObject::InOrderTraversalIterator iter = root->GetInOrderTraversalIterator();
  EXPECT_EQ(*root, *iter);
  ++iter;  // Skip the generic container which is an ignored object.
  EXPECT_NE(GetAXObjectCache().InOrderTraversalEnd(), iter);
  EXPECT_EQ(*button, *++iter);
  EXPECT_EQ(AccessibilityRole::kButtonRole, iter->RoleValue());
  EXPECT_EQ(*button, *iter++);
  EXPECT_EQ(GetAXObjectCache().InOrderTraversalEnd(), iter);
  EXPECT_EQ(*button, *--iter);
  EXPECT_EQ(*button, *iter--);
  --iter;  // Skip the generic container which is an ignored object.
  EXPECT_EQ(AccessibilityRole::kWebAreaRole, iter->RoleValue());
  EXPECT_EQ(GetAXObjectCache().InOrderTraversalBegin(), iter);
}

}  // namespace blink
