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
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

namespace {

unsigned PreviousSentencePositionBoundary(const UChar* characters,
                                          unsigned length,
                                          unsigned,
                                          BoundarySearchContextAvailability,
                                          bool&) {
  // FIXME: This is identical to startSentenceBoundary. I'm pretty sure that's
  // not right.
  TextBreakIterator* iterator = SentenceBreakIterator(characters, length);
  // FIXME: The following function can return -1; we don't handle that.
  return iterator->preceding(length);
}

unsigned StartSentenceBoundary(const UChar* characters,
                               unsigned length,
                               unsigned,
                               BoundarySearchContextAvailability,
                               bool&) {
  TextBreakIterator* iterator = SentenceBreakIterator(characters, length);
  // FIXME: The following function can return -1; we don't handle that.
  return iterator->preceding(length);
}

// TODO(yosin) This includes the space after the punctuation that marks the end
// of the sentence.
PositionInFlatTree EndOfSentenceInternal(const PositionInFlatTree& position) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   public:
    Position Find(const String text, unsigned passed_offset) final {
      DCHECK_LE(passed_offset, text.length());
      TextBreakIterator* iterator =
          SentenceBreakIterator(text.Characters16(), text.length());
      // "move_by_sentence_boundary.html" requires to skip a space characters
      // between sentences.
      const unsigned offset = FindNonSpaceCharacter(text, passed_offset);
      const int result = iterator->following(offset);
      if (result == kTextBreakDone)
        return Position();
      return result == 0 ? Position::Before(0) : Position::After(result - 1);
    }

   private:
    static unsigned FindNonSpaceCharacter(const String text,
                                          unsigned passed_offset) {
      for (unsigned offset = passed_offset; offset < text.length(); ++offset) {
        if (text[offset] != ' ')
          return offset;
      }
      return text.length();
    }
  } finder;
  return TextSegments::FindBoundaryForward(position, &finder);
}

PositionInFlatTree NextSentencePositionInternal(
    const PositionInFlatTree& position) {
  class Finder final : public TextSegments::Finder {
    STACK_ALLOCATED();

   private:
    Position Find(const String text, unsigned offset) final {
      DCHECK_LE(offset, text.length());
      if (should_stop_finding_) {
        DCHECK_EQ(offset, 0u);
        return Position::Before(0);
      }
      if (IsImplicitEndOfSentence(text, offset)) {
        // Since each block is separated by newline == end of sentence code,
        // |Find()| will stop at start of next block rater than between blocks.
        should_stop_finding_ = true;
        return Position();
      }
      TextBreakIterator* it =
          SentenceBreakIterator(text.Characters16(), text.length());
      const int result = it->following(offset);
      if (result == kTextBreakDone)
        return Position();
      return result == 0 ? Position::Before(0) : Position::After(result - 1);
    }

    static bool IsImplicitEndOfSentence(const String text, unsigned offset) {
      DCHECK_LE(offset, text.length());
      if (offset == text.length()) {
        // "extend-by-sentence-002.html" reaches here.
        // Example: <p>abc|</p><p>def</p> => <p>abc</p><p>|def</p>
        return true;
      }
      if (offset + 1 == text.length() && text[offset] == '\n') {
        // "move_forward_sentence_empty_line_break.html" reaches here.
        // foo<div>|<br></div>bar -> foo<div><br></div>|bar
        return true;
      }
      return false;
    }

    bool should_stop_finding_ = false;
  } finder;
  return TextSegments::FindBoundaryForward(position, &finder);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> StartOfSentenceAlgorithm(
    const VisiblePositionTemplate<Strategy>& c) {
  DCHECK(c.IsValid()) << c;
  return CreateVisiblePosition(PreviousBoundary(c, StartSentenceBoundary));
}

}  // namespace

PositionInFlatTreeWithAffinity EndOfSentence(const PositionInFlatTree& start) {
  const PositionInFlatTree result = EndOfSentenceInternal(start);
  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
      PositionInFlatTreeWithAffinity(result), start);
}

PositionWithAffinity EndOfSentence(const Position& start) {
  const PositionInFlatTreeWithAffinity result =
      EndOfSentence(ToPositionInFlatTree(start));
  return ToPositionInDOMTreeWithAffinity(result);
}

VisiblePosition EndOfSentence(const VisiblePosition& c) {
  return CreateVisiblePosition(EndOfSentence(c.DeepEquivalent()));
}

VisiblePositionInFlatTree EndOfSentence(const VisiblePositionInFlatTree& c) {
  return CreateVisiblePosition(EndOfSentence(c.DeepEquivalent()));
}

EphemeralRange ExpandEndToSentenceBoundary(const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  const VisiblePosition& visible_end =
      CreateVisiblePosition(range.EndPosition());
  DCHECK(visible_end.IsNotNull());
  const Position& sentence_end = EndOfSentence(visible_end).DeepEquivalent();
  // TODO(editing-dev): |sentenceEnd < range.endPosition()| is possible,
  // which would trigger a DCHECK in EphemeralRange's constructor if we return
  // it directly. However, this shouldn't happen and needs to be fixed.
  return EphemeralRange(
      range.StartPosition(),
      sentence_end.IsNotNull() && sentence_end > range.EndPosition()
          ? sentence_end
          : range.EndPosition());
}

EphemeralRange ExpandRangeToSentenceBoundary(const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  const VisiblePosition& visible_start =
      CreateVisiblePosition(range.StartPosition());
  DCHECK(visible_start.IsNotNull());
  const Position& sentence_start =
      StartOfSentence(visible_start).DeepEquivalent();
  // TODO(editing-dev): |sentenceStart > range.startPosition()| is possible,
  // which would trigger a DCHECK in EphemeralRange's constructor if we return
  // it directly. However, this shouldn't happen and needs to be fixed.
  return ExpandEndToSentenceBoundary(EphemeralRange(
      sentence_start.IsNotNull() && sentence_start < range.StartPosition()
          ? sentence_start
          : range.StartPosition(),
      range.EndPosition()));
}

// ----

PositionInFlatTreeWithAffinity NextSentencePosition(
    const PositionInFlatTree& start) {
  const PositionInFlatTree result = NextSentencePositionInternal(start);
  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
      PositionInFlatTreeWithAffinity(result), start);
}

PositionWithAffinity NextSentencePosition(const Position& start) {
  const PositionInFlatTreeWithAffinity result =
      NextSentencePosition(ToPositionInFlatTree(start));
  return ToPositionInDOMTreeWithAffinity(result);
}

VisiblePosition NextSentencePosition(const VisiblePosition& c) {
  return CreateVisiblePosition(
      NextSentencePosition(c.DeepEquivalent()).GetPosition(),
      TextAffinity::kUpstreamIfPossible);
}

VisiblePositionInFlatTree NextSentencePosition(
    const VisiblePositionInFlatTree& c) {
  return CreateVisiblePosition(
      NextSentencePosition(c.DeepEquivalent()).GetPosition(),
      TextAffinity::kUpstreamIfPossible);
}

// ----

VisiblePosition PreviousSentencePosition(const VisiblePosition& c) {
  DCHECK(c.IsValid()) << c;
  VisiblePosition prev = CreateVisiblePosition(
      PreviousBoundary(c, PreviousSentencePositionBoundary));
  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
      prev, c.DeepEquivalent());
}

VisiblePosition StartOfSentence(const VisiblePosition& c) {
  return StartOfSentenceAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree StartOfSentence(const VisiblePositionInFlatTree& c) {
  return StartOfSentenceAlgorithm<EditingInFlatTreeStrategy>(c);
}

}  // namespace blink
