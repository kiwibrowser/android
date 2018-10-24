/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/text_segments.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

namespace {

PositionInFlatTree EndOfWordPositionInternal(const PositionInFlatTree& position,
                                             EWordSide side) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   public:
    Finder(EWordSide side) : side_(side) {}

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (!is_first_time_)
        return FindInternal(text, offset);
      is_first_time_ = false;
      if (side_ == kPreviousWordIfOnBoundary) {
        if (offset == 0)
          return Position::Before(0);
        return FindInternal(text, offset - 1);
      }
      if (offset == text.length())
        return Position::After(offset);
      return FindInternal(text, offset);
    }

    static Position FindInternal(const String text, unsigned offset) {
      DCHECK_LE(offset, text.length());
      TextBreakIterator* it =
          WordBreakIterator(text.Characters16(), text.length());
      const int result = it->following(offset);
      if (result == kTextBreakDone || result == 0)
        return Position();
      return Position::After(result - 1);
    }

    const EWordSide side_;
    bool is_first_time_ = true;
  } finder(side);
  return TextSegments::FindBoundaryForward(position, &finder);
}

PositionInFlatTree NextWordPositionInternal(
    const PositionInFlatTree& position) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (offset == text.length() || text.length() == 0)
        return Position();
      TextBreakIterator* it =
          WordBreakIterator(text.Characters16(), text.length());
      for (int runner = it->following(offset); runner != kTextBreakDone;
           runner = it->following(runner)) {
        // We stop searching when the character preceding the break is
        // alphanumeric or underscore.
        if (static_cast<unsigned>(runner) < text.length() &&
            (WTF::Unicode::IsAlphanumeric(text[runner - 1]) ||
             text[runner - 1] == kLowLineCharacter))
          return Position::After(runner - 1);
      }
      return Position::After(text.length() - 1);
    }
  } finder;
  return TextSegments::FindBoundaryForward(position, &finder);
}

unsigned PreviousWordPositionBoundary(
    const UChar* characters,
    unsigned length,
    unsigned offset,
    BoundarySearchContextAvailability may_have_more_context,
    bool& need_more_context) {
  if (may_have_more_context &&
      !StartOfLastWordBoundaryContext(characters, offset)) {
    need_more_context = true;
    return 0;
  }
  need_more_context = false;
  return FindNextWordBackward(characters, length, offset);
}

unsigned StartWordBoundary(
    const UChar* characters,
    unsigned length,
    unsigned offset,
    BoundarySearchContextAvailability may_have_more_context,
    bool& need_more_context) {
  TRACE_EVENT0("blink", "startWordBoundary");
  DCHECK(offset);
  if (may_have_more_context &&
      !StartOfLastWordBoundaryContext(characters, offset)) {
    need_more_context = true;
    return 0;
  }
  need_more_context = false;
  U16_BACK_1(characters, 0, offset);
  return FindWordStartBoundary(characters, length, offset);
}

template <typename Strategy>
PositionTemplate<Strategy> StartOfWordAlgorithm(
    const VisiblePositionTemplate<Strategy>& c,
    EWordSide side) {
  DCHECK(c.IsValid()) << c;
  // TODO(yosin) This returns a null VP for c at the start of the document
  // and |side| == |kPreviousWordIfOnBoundary|
  VisiblePositionTemplate<Strategy> p = c;
  if (side == kNextWordIfOnBoundary) {
    // at paragraph end, the startofWord is the current position
    if (IsEndOfParagraph(c))
      return c.DeepEquivalent();

    p = NextPositionOf(c);
    if (p.IsNull())
      return c.DeepEquivalent();
  }
  return PreviousBoundary(p, StartWordBoundary);
}

}  // namespace

PositionInFlatTree EndOfWordPosition(const PositionInFlatTree& start,
                                     EWordSide side) {
  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
             PositionInFlatTreeWithAffinity(
                 EndOfWordPositionInternal(start, side)),
             start)
      .GetPosition();
}

Position EndOfWordPosition(const Position& position, EWordSide side) {
  return ToPositionInDOMTree(
      EndOfWordPosition(ToPositionInFlatTree(position), side));
}

VisiblePosition EndOfWord(const VisiblePosition& position, EWordSide side) {
  return CreateVisiblePosition(
      EndOfWordPosition(position.DeepEquivalent(), side),
      TextAffinity::kUpstreamIfPossible);
}

VisiblePositionInFlatTree EndOfWord(const VisiblePositionInFlatTree& position,
                                    EWordSide side) {
  return CreateVisiblePosition(
      EndOfWordPosition(position.DeepEquivalent(), side),
      TextAffinity::kUpstreamIfPossible);
}

// ----
// TODO(editing-dev): Because of word boundary can not be an upstream position,
// we should make this function to return |PositionInFlatTree|.
PositionInFlatTreeWithAffinity NextWordPosition(
    const PositionInFlatTree& start) {
  const PositionInFlatTree next = NextWordPositionInternal(start);
  // Note: The word boundary can not be upstream position.
  const PositionInFlatTreeWithAffinity adjusted =
      AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          PositionInFlatTreeWithAffinity(next), start);
  DCHECK_EQ(adjusted.Affinity(), TextAffinity::kDownstream);
  return adjusted;
}

PositionWithAffinity NextWordPosition(const Position& start) {
  const PositionInFlatTreeWithAffinity& next =
      NextWordPosition(ToPositionInFlatTree(start));
  return ToPositionInDOMTreeWithAffinity(next);
}

// TODO(yosin): This function will be removed by replacing call sites to use
// |Position| version. since there are only two call sites, one is in test.
VisiblePosition NextWordPosition(const VisiblePosition& c) {
  DCHECK(c.IsValid()) << c;
  return CreateVisiblePosition(NextWordPosition(c.DeepEquivalent()));
}

VisiblePosition PreviousWordPosition(const VisiblePosition& c) {
  DCHECK(c.IsValid()) << c;
  VisiblePosition prev =
      CreateVisiblePosition(PreviousBoundary(c, PreviousWordPositionBoundary));
  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
      prev, c.DeepEquivalent());
}

Position StartOfWordPosition(const VisiblePosition& position, EWordSide side) {
  return StartOfWordAlgorithm<EditingStrategy>(position, side);
}

VisiblePosition StartOfWord(const VisiblePosition& position, EWordSide side) {
  return CreateVisiblePosition(StartOfWordPosition(position, side));
}

PositionInFlatTree StartOfWordPosition(
    const VisiblePositionInFlatTree& position,
    EWordSide side) {
  return StartOfWordAlgorithm<EditingInFlatTreeStrategy>(position, side);
}

VisiblePositionInFlatTree StartOfWord(const VisiblePositionInFlatTree& position,
                                      EWordSide side) {
  return CreateVisiblePosition(StartOfWordPosition(position, side));
}

}  // namespace blink
