// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_selection_test.h"

#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Serialize accessibility subtree to selection text.
// Adds a '^' at the selection anchor offset and a '|' at the focus offset.
class AXSelectionSerializer final {
  STACK_ALLOCATED();

 public:
  explicit AXSelectionSerializer(const AXSelection& selection)
      : selection_(selection) {}

  std::string Serialize(const AXObject& subtree) {
    if (!selection_.IsValid())
      return {};
    SerializeSubtree(subtree);
    return builder_.ToString().Utf8().data();
  }

 private:
  void HandleTextObject(const AXObject& text_object) {
    builder_.Append('<');
    builder_.Append(AXObject::InternalRoleName(text_object.RoleValue()));
    builder_.Append(": ");
    const String name = text_object.ComputedName() + '>';
    const AXObject& base_container = *selection_.Base().ContainerObject();
    const AXObject& extent_container = *selection_.Extent().ContainerObject();

    if (base_container == text_object && extent_container == text_object) {
      DCHECK(selection_.Base().IsTextPosition() &&
             selection_.Extent().IsTextPosition());
      const int base_offset = selection_.Base().TextOffset();
      const int extent_offset = selection_.Extent().TextOffset();

      if (base_offset == extent_offset) {
        builder_.Append(name.Left(base_offset));
        builder_.Append('|');
        builder_.Append(name.Substring(base_offset));
        return;
      }

      if (base_offset < extent_offset) {
        builder_.Append(name.Left(base_offset));
        builder_.Append('^');
        builder_.Append(
            name.Substring(base_offset, extent_offset - base_offset));
        builder_.Append('|');
        builder_.Append(name.Substring(extent_offset));
        return;
      }

      builder_.Append(name.Left(extent_offset));
      builder_.Append('|');
      builder_.Append(
          name.Substring(extent_offset, base_offset - extent_offset));
      builder_.Append('^');
      builder_.Append(name.Substring(base_offset));
      return;
    }

    if (base_container == text_object) {
      DCHECK(selection_.Base().IsTextPosition());
      const int base_offset = selection_.Base().TextOffset();

      builder_.Append(name.Left(base_offset));
      builder_.Append('^');
      builder_.Append(name.Substring(base_offset));
      return;
    }

    if (extent_container == text_object) {
      DCHECK(selection_.Extent().IsTextPosition());
      const int extent_offset = selection_.Extent().TextOffset();

      builder_.Append(name.Left(extent_offset));
      builder_.Append('|');
      builder_.Append(name.Substring(extent_offset));
      return;
    }

    builder_.Append(name);
  }

  void HandleObject(const AXObject& object) {
    builder_.Append('<');
    builder_.Append(AXObject::InternalRoleName(object.RoleValue()));
    builder_.Append(": ");
    builder_.Append(object.ComputedName());
    builder_.Append('>');
    SerializeSubtree(object);
  }

  void HandleSelection(const AXPosition& position) {
    if (!position.IsValid())
      return;

    if (selection_.Extent() == position) {
      builder_.Append('|');
      return;
    }

    if (selection_.Base() != position)
      return;

    builder_.Append('^');
  }

  void SerializeSubtree(const AXObject& subtree) {
    for (const Member<AXObject>& child : subtree.Children()) {
      if (!child)
        continue;
      const auto position = AXPosition::CreatePositionBeforeObject(*child);
      HandleSelection(position);
      if (position.IsTextPosition()) {
        HandleTextObject(*child);
      } else {
        HandleObject(*child);
      }
    }
    HandleSelection(AXPosition::CreateLastPositionInObject(subtree));
  }

  StringBuilder builder_;
  AXSelection selection_;
};

}  // namespace

AccessibilitySelectionTest::AccessibilitySelectionTest(
    LocalFrameClient* local_frame_client)
    : AccessibilityTest(local_frame_client) {}

std::string AccessibilitySelectionTest::GetSelectionText(
    const AXSelection& selection) const {
  const AXObject* root = GetAXRootObject();
  if (!root)
    return {};
  return AXSelectionSerializer(selection).Serialize(*root);
}

std::string AccessibilitySelectionTest::GetSelectionText(
    const AXSelection& selection,
    const AXObject& subtree) const {
  return AXSelectionSerializer(selection).Serialize(subtree);
}

}  // namespace blink
