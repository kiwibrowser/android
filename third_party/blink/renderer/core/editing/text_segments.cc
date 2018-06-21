// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/text_segments.h"

#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/text_offset_mapping.h"

namespace blink {

TextSegments::Finder::Position::Position() = default;

TextSegments::Finder::Position::Position(unsigned offset, Type type)
    : offset_(offset), type_(type) {
  DCHECK_NE(type, kNone);
}

// static
TextSegments::Finder::Position TextSegments::Finder::Position::Before(
    unsigned offset) {
  return Position(offset, kBefore);
}

// static
TextSegments::Finder::Position TextSegments::Finder::Position::After(
    unsigned offset) {
  return Position(offset, kAfter);
}

unsigned TextSegments::Finder::Position::Offset() const {
  DCHECK(type_ == kBefore || type_ == kAfter) << type_;
  return offset_;
}

// static
PositionInFlatTree TextSegments::FindBoundaryForward(
    const PositionInFlatTree& position,
    Finder* finder) {
  DCHECK(position.IsNotNull());
  PositionInFlatTree last_position = position;
  for (const auto& inline_contents :
       TextOffsetMapping::ForwardRangeOf(position)) {
    const TextOffsetMapping mapping(inline_contents);
    const String text = mapping.GetText();
    const unsigned offset =
        last_position == position ? mapping.ComputeTextOffset(position) : 0;
    const TextSegments::Finder::Position result = finder->Find(text, offset);
    if (result.IsBefore())
      return mapping.GetPositionBefore(result.Offset());
    if (result.IsAfter())
      return mapping.GetPositionAfter(result.Offset());
    DCHECK(result.IsNone());
    last_position = mapping.GetRange().EndPosition();
  }
  return last_position;
}

}  // namespace blink
