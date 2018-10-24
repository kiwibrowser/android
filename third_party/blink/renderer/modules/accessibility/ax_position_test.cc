// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_position.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

constexpr char kHTMLTable[] = R"HTML(
    <p id="before">Before table.</p>
    <table id="table" border="1">
      <thead>
        <tr id="headerRow">
          <th id="firstHeaderCell">Number</th>
          <th>Month</th>
          <th id="lastHeaderCell">Expenses</th>
        </tr>
      </thead>
      <tbody>
        <tr id="firstRow">
          <th id="firstCell">1</th>
          <td>Jan</td>
          <td>100</td>
        </tr>
        <tr>
          <th>2</th>
          <td>Feb</td>
          <td>150</td>
        </tr>
        <tr id="lastRow">
          <th>3</th>
          <td>Mar</td>
          <td id="lastCell">200</td>
        </tr>
      </tbody>
    </table>
    <p id="after">After table.</p>
    )HTML";

constexpr char kAOM[] = R"HTML(
    <p id="before">Before virtual AOM node.</p>
    <div id="aomParent"></div>
    <p id="after">After virtual AOM node.</p>
    <script>
      let parent = document.getElementById("aomParent");
      let node = new AccessibleNode();
      node.role = "button";
      node.label = "Button";
      parent.accessibleNode.appendChild(node);
    </script>
    )HTML";

}  // namespace

//
// Basic tests.
//

TEST_F(AccessibilityTest, PositionInText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(3, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

// To prevent surprises when comparing equality of two |AXPosition|s, position
// before text object should be the same as position in text object at offset 0.
TEST_F(AccessibilityTest, PositionBeforeText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeTextWithFirstLetterCSSRule) {
  SetBodyInnerHTML(
      R"HTML(<style>p ::first-letter { color: red; font-size: 200%; }</style>
      <p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

// To prevent surprises when comparing equality of two |AXPosition|s, position
// after text object should be the same as position in text object at offset
// text length.
TEST_F(AccessibilityTest, PositionAfterText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionAfterObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeLineBreak) {
  SetBodyInnerHTML(R"HTML(Hello<br id="br">there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(1, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_br, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionAfterLineBreak) {
  SetBodyInnerHTML(R"HTML(Hello<br id="br">there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestLastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(2, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, FirstPositionInDivContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello<br>there</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestFirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreateFirstPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsBeforeChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInDivContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello<br>there</div>
                   <div>Next div</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());

  const auto ax_position = AXPosition::CreateLastPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsAfterChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, FirstPositionInTextContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello</div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateFirstPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInTextContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello</div>)HTML");
  const Node* text = GetElementById("div")->lastChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->LastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateLastPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

//
// Test comparing two AXPosition objects based on their position in the
// accessibility tree.
//

TEST_F(AccessibilityTest, AXPositionComparisonOperators) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <p id="paragraph">hello<br>there</p>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const auto root_first = AXPosition::CreateFirstPositionInObject(*root);
  const auto root_last = AXPosition::CreateLastPositionInObject(*root);

  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const auto input_before = AXPosition::CreatePositionBeforeObject(*input);
  const auto input_after = AXPosition::CreatePositionAfterObject(*input);

  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  ASSERT_NE(nullptr, paragraph->FirstChild());
  ASSERT_NE(nullptr, paragraph->LastChild());
  const auto paragraph_before =
      AXPosition::CreatePositionBeforeObject(*paragraph->FirstChild());
  const auto paragraph_after =
      AXPosition::CreatePositionAfterObject(*paragraph->LastChild());
  const auto paragraph_start =
      AXPosition::CreatePositionInTextObject(*paragraph->FirstChild(), 0);
  const auto paragraph_end =
      AXPosition::CreatePositionInTextObject(*paragraph->LastChild(), 5);

  EXPECT_TRUE(root_first == root_first);
  EXPECT_TRUE(root_last == root_last);
  EXPECT_FALSE(root_first != root_first);
  EXPECT_TRUE(root_first != root_last);

  EXPECT_TRUE(root_first < root_last);
  EXPECT_TRUE(root_first <= root_first);
  EXPECT_TRUE(root_last > root_first);
  EXPECT_TRUE(root_last >= root_last);

  EXPECT_TRUE(input_before == root_first);
  EXPECT_TRUE(input_after > root_first);
  EXPECT_TRUE(input_after >= root_first);
  EXPECT_FALSE(input_before < root_first);
  EXPECT_TRUE(input_before <= root_first);

  //
  // Text positions.
  //

  EXPECT_TRUE(paragraph_before == paragraph_start);
  EXPECT_TRUE(paragraph_after == paragraph_end);
  EXPECT_TRUE(paragraph_start < paragraph_end);
}

TEST_F(AccessibilityTest, AXPositionOperatorBool) {
  SetBodyInnerHTML(R"HTML(Hello)HTML");
  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const auto root_first = AXPosition::CreateFirstPositionInObject(*root);
  EXPECT_TRUE(static_cast<bool>(root_first));
  // The following should create an after children position on the root so it
  // should be valid.
  EXPECT_TRUE(static_cast<bool>(root_first.CreateNextPosition()));
  EXPECT_FALSE(static_cast<bool>(root_first.CreatePreviousPosition()));
}

//
// Test converting to and from visible text with white space.
// The accessibility tree is based on visible text with white space compressed,
// vs. the DOM tree where white space is preserved.
//

TEST_F(AccessibilityTest, PositionInTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(8, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionAfterTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->lastChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->LastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionAfterObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(10, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeLineBreakWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(Hello     <br id="br">     there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(1, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_br, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionAfterLineBreakWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(Hello     <br id="br">     there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestLastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(2, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, FirstPositionInDivContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello<br>there     </div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestFirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreateFirstPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsBeforeChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInDivContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello<br>there     </div>
                   <div>Next div</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());

  const auto ax_position = AXPosition::CreateLastPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsAfterChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, FirstPositionInTextContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateFirstPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInTextContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->lastChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->LastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateLastPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(10, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

// Test that DOM positions in white space will be collapsed to the first or last
// valid offset in an |AXPosition|.
TEST_F(AccessibilityTest, AXPositionFromDOMPositionWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  ASSERT_EQ(15U, text->textContent().length());
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const Position position_at_start(*text, 0);
  const auto ax_position_at_start = AXPosition::FromPosition(position_at_start);
  EXPECT_TRUE(ax_position_at_start.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_at_start.ContainerObject());
  EXPECT_EQ(0, ax_position_at_start.TextOffset());
  EXPECT_EQ(nullptr, ax_position_at_start.ChildAfterTreePosition());

  const Position position_after_white_space(*text, 5);
  const auto ax_position_after_white_space =
      AXPosition::FromPosition(position_after_white_space);
  EXPECT_TRUE(ax_position_after_white_space.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_after_white_space.ContainerObject());
  EXPECT_EQ(0, ax_position_after_white_space.TextOffset());
  EXPECT_EQ(nullptr, ax_position_after_white_space.ChildAfterTreePosition());

  const Position position_at_end(*text, 15);
  const auto ax_position_at_end = AXPosition::FromPosition(position_at_end);
  EXPECT_TRUE(ax_position_at_end.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_at_end.ContainerObject());
  EXPECT_EQ(5, ax_position_at_end.TextOffset());
  EXPECT_EQ(nullptr, ax_position_at_end.ChildAfterTreePosition());

  const Position position_before_white_space(*text, 10);
  const auto ax_position_before_white_space =
      AXPosition::FromPosition(position_before_white_space);
  EXPECT_TRUE(ax_position_before_white_space.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_before_white_space.ContainerObject());
  EXPECT_EQ(5, ax_position_before_white_space.TextOffset());
  EXPECT_EQ(nullptr, ax_position_before_white_space.ChildAfterTreePosition());
}

//
// Test affinity.
// We need to distinguish between the caret at the end of one line and the
// beginning of the next.
//

TEST_F(AccessibilityTest, PositionInTextWithAffinity) {}

TEST_F(AccessibilityTest, PositionFromTextPositionWithAffinity) {}

TEST_F(AccessibilityTest, PositionInTextWithAffinityAndWhiteSpace) {}

TEST_F(AccessibilityTest, PositionFromTextPositionWithAffinityAndWhiteSpace) {}

//
// Test converting to and from accessibility positions with offsets in labels
// and alt text. Alt text, aria-label and other ARIA relationships can cause the
// accessible name of an object to be different than its DOM text.
//

TEST_F(AccessibilityTest, PositionInHTMLLabel) {}

TEST_F(AccessibilityTest, PositionInARIALabel) {}

TEST_F(AccessibilityTest, PositionInARIALabelledBy) {}

TEST_F(AccessibilityTest, PositionInPlaceholder) {}

TEST_F(AccessibilityTest, PositionInAltText) {}

TEST_F(AccessibilityTest, PositionInTitle) {}

//
// Some objects are accessibility ignored.
//

TEST_F(AccessibilityTest, PositionInIgnoredObject) {}

//
// Aria-hidden can cause things in the DOM to be hidden from accessibility.
//

TEST_F(AccessibilityTest, BeforePositionInARIAHiddenShouldSkipARIAHidden) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before aria-hidden.</p>
      <p id="ariaHidden" aria-hidden="true">Aria-hidden.</p>
      <p id="after">After aria-hidden.</p>
      )HTML");
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);
  const AXObject* ax_before = GetAXObjectByElementId("before");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(AccessibilityRole::kParagraphRole, ax_before->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(AccessibilityRole::kParagraphRole, ax_after->RoleValue());
  ASSERT_NE(nullptr, GetAXObjectByElementId("ariaHidden"));
  ASSERT_TRUE(GetAXObjectByElementId("ariaHidden")->AccessibilityIsIgnored());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_before);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(after, position.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_after, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PreviousPositionAfterARIAHiddenShouldSkipARIAHidden) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before aria-hidden.</p>
      <p id="ariaHidden" aria-hidden="true">Aria-hidden.</p>
      <p id="after">After aria-hidden.</p>
      )HTML");
  const Node* before = GetElementById("before");
  ASSERT_NE(nullptr, before);
  ASSERT_NE(nullptr, before->firstChild());
  ASSERT_TRUE(before->firstChild()->IsTextNode());
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(AccessibilityRole::kParagraphRole, ax_after->RoleValue());
  ASSERT_NE(nullptr, GetAXObjectByElementId("ariaHidden"));
  ASSERT_TRUE(GetAXObjectByElementId("ariaHidden")->AccessibilityIsIgnored());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_after);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(after, position.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_after, ax_position_from_dom.ChildAfterTreePosition());

  const auto ax_position_previous = ax_position.CreatePreviousPosition();
  const auto position_previous = ax_position_previous.ToPositionWithAffinity();
  EXPECT_EQ(before->firstChild(), position_previous.AnchorNode());
  EXPECT_EQ(19, position_previous.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(nullptr,
            position_previous.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_previous_from_dom =
      AXPosition::FromPosition(position_previous);
  EXPECT_EQ(ax_position_previous, ax_position_previous_from_dom);
  EXPECT_EQ(nullptr, ax_position_previous_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, DISABLED_FromPositionInARIAHidden) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before aria-hidden.</p>
      <p id="ariaHidden" aria-hidden="true">Aria-hidden.</p>
      <p id="after">After aria-hidden.</p>
      )HTML");
  const Node* hidden = GetElementById("ariaHidden");
  ASSERT_NE(nullptr, hidden);
  const Node* text = hidden->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_root = GetAXRootObject();
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(AccessibilityRole::kWebAreaRole, ax_root->RoleValue());
  ASSERT_EQ(2, ax_root->ChildCount());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(AccessibilityRole::kParagraphRole, ax_after->RoleValue());
  ASSERT_NE(nullptr, GetAXObjectByElementId("ariaHidden"));
  ASSERT_TRUE(GetAXObjectByElementId("ariaHidden")->AccessibilityIsIgnored());

  const auto position_first = Position::FirstPositionInNode(*hidden);
  const auto position_last = Position::LastPositionInNode(*hidden);
  const auto position_before = Position::BeforeNode(*hidden);
  const auto position_after = Position::AfterNode(*hidden);
  const auto position_text = Position::EditingPositionOf(text, 3);
  const auto positions = {position_first, position_last, position_before,
                          position_after, position_text};

  for (const auto& position : positions) {
    const auto ax_position_left = AXPosition::FromPosition(position);
    EXPECT_TRUE(ax_position_left.IsValid());
    EXPECT_FALSE(ax_position_left.IsTextPosition());
    EXPECT_EQ(ax_root, ax_position_left.ContainerObject());
    EXPECT_EQ(1, ax_position_left.ChildIndex());
    EXPECT_EQ(ax_after, ax_position_left.ChildAfterTreePosition());

    const auto ax_position_right = AXPosition::FromPosition(position);
    EXPECT_TRUE(ax_position_right.IsValid());
    EXPECT_FALSE(ax_position_right.IsTextPosition());
    EXPECT_EQ(ax_root, ax_position_left.ContainerObject());
    EXPECT_EQ(2, ax_position_left.ChildIndex());
    EXPECT_EQ(nullptr, ax_position_left.ChildAfterTreePosition());
  }
}

//
// Canvas fallback can cause things to be in the accessibility tree that are not
// in the layout tree.
//

TEST_F(AccessibilityTest, PositionInCanvas) {}

//
// Some layout objects, e.g. list bullets and CSS::before/after content, appear
// in the accessibility tree but are not present in the DOM.
//

TEST_F(AccessibilityTest, DISABLED_PositionBeforeListMarker) {
  SetBodyInnerHTML(R"HTML(
      <ul id="list">
        <li id="listItem">Item.</li>
      </ul>
      )HTML");
  const Node* list = GetElementById("list");
  ASSERT_NE(nullptr, list);
  const Node* item = GetElementById("listItem");
  ASSERT_NE(nullptr, item);
  const Node* text = item->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_item = GetAXObjectByElementId("listItem");
  ASSERT_NE(nullptr, ax_item);
  ASSERT_EQ(AccessibilityRole::kListItemRole, ax_item->RoleValue());
  ASSERT_EQ(2, ax_item->ChildCount());
  const AXObject* ax_marker = ax_item->FirstChild();
  ASSERT_NE(nullptr, ax_marker);
  ASSERT_EQ(AccessibilityRole::kListMarkerRole, ax_marker->RoleValue());

  const auto ax_position_1 = AXPosition::CreateFirstPositionInObject(*ax_item);
  const auto position_1 = ax_position_1.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(list, position_1.AnchorNode());
  EXPECT_TRUE(position_1.GetPosition().IsBeforeChildren());
  EXPECT_EQ(item, position_1.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom_1 = AXPosition::FromPosition(position_1);
  EXPECT_EQ(ax_position_1, ax_position_from_dom_1);
  EXPECT_EQ(ax_marker, ax_position_from_dom_1.ChildAfterTreePosition());

  const auto ax_position_2 = AXPosition::CreatePositionBeforeObject(*ax_marker);
  const auto position_2 = ax_position_2.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(list, position_2.AnchorNode());
  EXPECT_TRUE(position_2.GetPosition().IsBeforeChildren());
  EXPECT_EQ(item, position_2.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom_2 = AXPosition::FromPosition(position_2);
  EXPECT_EQ(ax_position_2, ax_position_from_dom_2);
  EXPECT_EQ(ax_marker, ax_position_from_dom_2.ChildAfterTreePosition());

  const auto position_3 = ax_position_1.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(list, position_3.AnchorNode());
  EXPECT_TRUE(position_3.GetPosition().IsBeforeChildren());
  EXPECT_EQ(item, position_3.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom_3 = AXPosition::FromPosition(position_3);
  EXPECT_EQ(ax_position_1, ax_position_from_dom_3);
  EXPECT_EQ(ax_marker, ax_position_from_dom_3.ChildAfterTreePosition());

  const auto position_4 = ax_position_2.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(list, position_4.AnchorNode());
  EXPECT_TRUE(position_4.GetPosition().IsBeforeChildren());
  EXPECT_EQ(item, position_4.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom_4 = AXPosition::FromPosition(position_4);
  EXPECT_EQ(ax_position_2, ax_position_from_dom_4);
  EXPECT_EQ(ax_marker, ax_position_from_dom_4.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionAfterListMarker) {
  SetBodyInnerHTML(R"HTML(
      <ol>
        <li id="listItem">Item.</li>
      </ol>
      )HTML");
  const Node* item = GetElementById("listItem");
  ASSERT_NE(nullptr, item);
  const Node* text = item->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_item = GetAXObjectByElementId("listItem");
  ASSERT_NE(nullptr, ax_item);
  ASSERT_EQ(AccessibilityRole::kListItemRole, ax_item->RoleValue());
  ASSERT_EQ(2, ax_item->ChildCount());
  const AXObject* ax_marker = ax_item->FirstChild();
  ASSERT_NE(nullptr, ax_marker);
  ASSERT_EQ(AccessibilityRole::kListMarkerRole, ax_marker->RoleValue());
  const AXObject* ax_text = ax_item->LastChild();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_marker);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(item, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(text, position.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_text, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionInCSSContent) {}

//
// Objects deriving from |AXMockObject|, e.g. table columns, are in the
// accessibility tree but are neither in the DOM or layout trees.
// Same for virtual nodes created using the Accessibility Object Model (AOM).
//

TEST_F(AccessibilityTest, PositionBeforeAndAfterTable) {
  SetBodyInnerHTML(kHTMLTable);
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);
  const AXObject* ax_table = GetAXObjectByElementId("table");
  ASSERT_NE(nullptr, ax_table);
  ASSERT_EQ(AccessibilityRole::kTableRole, ax_table->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(AccessibilityRole::kParagraphRole, ax_after->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_table);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position_before.AnchorNode());
  EXPECT_EQ(3, position_before.GetPosition().OffsetInContainerNode());
  const Node* table = position_before.GetPosition().ComputeNodeAfterPosition();
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(GetElementById("table"), table);

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_table);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position_after.AnchorNode());
  EXPECT_EQ(5, position_after.GetPosition().OffsetInContainerNode());
  const Node* node_after =
      position_after.GetPosition().ComputeNodeAfterPosition();
  EXPECT_EQ(after, node_after);

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
  EXPECT_EQ(ax_after, ax_position_after_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionAtStartAndEndOfTable) {
  SetBodyInnerHTML(kHTMLTable);
  const Node* table = GetElementById("table");
  ASSERT_NE(nullptr, table);
  const Node* last_cell = GetElementById("lastCell");
  ASSERT_NE(nullptr, last_cell);
  const Node* last_cell_text = last_cell->firstChild();
  ASSERT_NE(nullptr, last_cell_text);
  ASSERT_TRUE(last_cell_text->IsTextNode());
  const AXObject* ax_table = GetAXObjectByElementId("table");
  ASSERT_NE(nullptr, ax_table);
  ASSERT_EQ(AccessibilityRole::kTableRole, ax_table->RoleValue());

  const auto ax_position_at_start =
      AXPosition::CreateFirstPositionInObject(*ax_table);
  const auto position_at_start = ax_position_at_start.ToPositionWithAffinity();
  EXPECT_EQ(table, position_at_start.AnchorNode());
  EXPECT_TRUE(position_at_start.GetPosition().IsBeforeChildren());

  const auto ax_position_at_start_from_dom =
      AXPosition::FromPosition(position_at_start);
  EXPECT_EQ(ax_position_at_start, ax_position_at_start_from_dom);

  const auto ax_position_at_end =
      AXPosition::CreateLastPositionInObject(*ax_table);
  const auto position_at_end = ax_position_at_end.ToPositionWithAffinity();
  EXPECT_EQ(last_cell_text, position_at_end.AnchorNode());
  EXPECT_EQ(3, position_at_end.GetPosition().OffsetInContainerNode());

  const auto ax_position_at_end_from_dom =
      AXPosition::FromPosition(position_at_end);
  EXPECT_EQ(ax_position_at_end.AsValidDOMPosition(),
            ax_position_at_end_from_dom);
  EXPECT_EQ(nullptr, ax_position_at_end_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionInTableHeader) {
  SetBodyInnerHTML(kHTMLTable);
  const Node* header_row = GetElementById("headerRow");
  ASSERT_NE(nullptr, header_row);
  const AXObject* ax_first_header_cell =
      GetAXObjectByElementId("firstHeaderCell");
  ASSERT_NE(nullptr, ax_first_header_cell);
  ASSERT_EQ(AccessibilityRole::kColumnHeaderRole,
            ax_first_header_cell->RoleValue());
  const AXObject* ax_last_header_cell =
      GetAXObjectByElementId("lastHeaderCell");
  ASSERT_NE(nullptr, ax_last_header_cell);
  ASSERT_EQ(AccessibilityRole::kColumnHeaderRole,
            ax_last_header_cell->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_first_header_cell);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(header_row, position_before.AnchorNode());
  EXPECT_TRUE(position_before.GetPosition().IsBeforeChildren());

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_last_header_cell);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(header_row, position_after.AnchorNode());
  EXPECT_TRUE(position_after.GetPosition().IsAfterChildren());
  EXPECT_EQ(nullptr, position_after.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
}

TEST_F(AccessibilityTest, PositionInTableRow) {
  SetBodyInnerHTML(kHTMLTable);
  const Node* first_row = GetElementById("firstRow");
  ASSERT_NE(nullptr, first_row);
  const Node* last_row = GetElementById("lastRow");
  ASSERT_NE(nullptr, last_row);
  const AXObject* ax_first_cell = GetAXObjectByElementId("firstCell");
  ASSERT_NE(nullptr, ax_first_cell);
  ASSERT_EQ(AccessibilityRole::kRowHeaderRole, ax_first_cell->RoleValue());
  const AXObject* ax_last_cell = GetAXObjectByElementId("lastCell");
  ASSERT_NE(nullptr, ax_last_cell);
  ASSERT_EQ(AccessibilityRole::kCellRole, ax_last_cell->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_first_cell);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(first_row, position_before.AnchorNode());
  EXPECT_TRUE(position_before.GetPosition().IsBeforeChildren());

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_last_cell);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(last_row, position_after.AnchorNode());
  EXPECT_TRUE(position_after.GetPosition().IsAfterChildren());
  EXPECT_EQ(nullptr, position_after.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
}

TEST_F(AccessibilityTest, DISABLED_PositionInVirtualAOMNode) {
  ScopedAccessibilityObjectModelForTest(true);
  SetBodyInnerHTML(kAOM);
  const Node* parent = GetElementById("aomParent");
  ASSERT_NE(nullptr, parent);
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);
  const AXObject* ax_parent = GetAXObjectByElementId("aomParent");
  ASSERT_NE(nullptr, ax_parent);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_parent->RoleValue());
  ASSERT_EQ(1, ax_parent->ChildCount());
  const AXObject* ax_button = ax_parent->FirstChild();
  ASSERT_NE(nullptr, ax_button);
  ASSERT_EQ(AccessibilityRole::kButtonRole, ax_button->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(AccessibilityRole::kParagraphRole, ax_after->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_button);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(parent, position_before.AnchorNode());
  EXPECT_TRUE(position_before.GetPosition().IsBeforeChildren());
  EXPECT_EQ(nullptr, position_before.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);
  EXPECT_EQ(ax_button, ax_position_before_from_dom.ChildAfterTreePosition());

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_button);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(after, position_after.AnchorNode());
  EXPECT_TRUE(position_after.GetPosition().IsBeforeChildren());
  EXPECT_EQ(nullptr, position_after.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
  EXPECT_EQ(ax_after, ax_position_after_from_dom.ChildAfterTreePosition());
}

}  // namespace blink
