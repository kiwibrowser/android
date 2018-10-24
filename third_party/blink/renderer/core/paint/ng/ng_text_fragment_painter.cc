// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"

#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/document_marker_painter.h"
#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

namespace blink {

namespace {

inline bool ShouldPaintTextFragment(const NGPhysicalTextFragment& text_fragment,
                                    const ComputedStyle& style) {
  if (style.Visibility() != EVisibility::kVisible)
    return false;

  // When painting selection, we want to include a highlight when the
  // selection spans line breaks. In other cases such as invisible elements
  // or those with no text that are not line breaks, we can skip painting
  // wholesale.
  // TODO(wkorman): Constrain line break painting to appropriate paint phase.
  // This code path is only called in PaintPhaseForeground whereas we would
  // expect PaintPhaseSelection. The existing haveSelection logic in paint()
  // tests for != PaintPhaseTextClip.
  if (text_fragment.IsLineBreak())
    return true;
  if (!text_fragment.Length() || !text_fragment.TextShapeResult())
    return false;

  return true;
}

Color SelectionBackgroundColor(const Document& document,
                               const ComputedStyle& style,
                               const NGPhysicalTextFragment& text_fragment,
                               Color text_color) {
  const Color color = SelectionPaintingUtils::SelectionBackgroundColor(
      document, style, text_fragment.GetNode());
  if (!color.Alpha())
    return Color();

  // If the text color ends up being the same as the selection background,
  // invert the selection background.
  if (text_color == color)
    return Color(0xff - color.Red(), 0xff - color.Green(), 0xff - color.Blue());
  return color;
}

DocumentMarkerVector ComputeMarkersToPaint(
    const NGPaintFragment& paint_fragment) {
  // TODO(yoichio): Handle first-letter
  Node* const node = paint_fragment.GetNode();
  if (!node)
    return DocumentMarkerVector();
  // We don't paint any marker on ellipsis.
  if (paint_fragment.PhysicalFragment().StyleVariant() ==
      NGStyleVariant::kEllipsis)
    return DocumentMarkerVector();

  DocumentMarkerController& document_marker_controller =
      node->GetDocument().Markers();
  return document_marker_controller.ComputeMarkersToPaint(*node);
}

void PaintStyleableMarkerUnderline(GraphicsContext& context,
                                   const LayoutPoint& box_origin,
                                   const StyleableMarker& marker,
                                   const ComputedStyle& style,
                                   const NGPhysicalOffsetRect& local_rect) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  DocumentMarkerPainter::PaintStyleableMarkerUnderline(
      context, box_origin, marker, style, local_rect.ToFloatRect(),
      LayoutUnit(font_data->GetFontMetrics().Height()));
}

unsigned GetTextContentOffset(const Text& text, unsigned offset) {
  const Position position(text, offset);
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const base::Optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  DCHECK(ng_offset.has_value());
  return ng_offset.value();
}

// ClampOffset modifies |offset| fixed in a range of |text_fragment| start/end
// offsets.
// |offset| points not each character but each span between character.
// With that concept, we can clear catch what is inside start / end.
// Suppose we have "foo_bar"('_' is a space).
// There are 8 offsets for that:
//  f o o _ b a r
// 0 1 2 3 4 5 6 7
// If "bar" is a TextFragment. That start(), end() {4, 7} correspond this
// offset. If a marker has StartOffset / EndOffset as {2, 6},
// ClampOffset returns{ 4,6 }, which represents "ba" on "foo_bar".
unsigned ClampOffset(unsigned offset,
                     const NGPhysicalTextFragment& text_fragment) {
  return std::min(std::max(offset, text_fragment.StartOffset()),
                  text_fragment.EndOffset());
}

void PaintRect(GraphicsContext& context,
               const NGPhysicalOffset& location,
               const NGPhysicalOffsetRect& rect,
               const Color color) {
  if (!color.Alpha())
    return;
  if (rect.size.IsEmpty())
    return;
  const NGPhysicalOffsetRect global_rect(rect.offset + location, rect.size);
  context.FillRect(global_rect.ToFloatRect(), color);
}

// Copied from InlineTextBoxPainter
void PaintDocumentMarkers(GraphicsContext& context,
                          const NGPaintFragment& paint_fragment,
                          const DocumentMarkerVector& markers_to_paint,
                          const LayoutPoint& box_origin,
                          const ComputedStyle& style,
                          DocumentMarkerPaintPhase marker_paint_phase,
                          NGTextPainter* text_painter) {
  if (markers_to_paint.IsEmpty())
    return;

  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragmentOrDie(paint_fragment.PhysicalFragment());
  DCHECK(text_fragment.GetNode());
  const Text& text = ToTextOrDie(*text_fragment.GetNode());
  for (const DocumentMarker* marker : markers_to_paint) {
    const unsigned marker_start_offset =
        GetTextContentOffset(text, marker->StartOffset());
    const unsigned marker_end_offset =
        GetTextContentOffset(text, marker->EndOffset());
    const unsigned paint_start_offset =
        ClampOffset(marker_start_offset, text_fragment);
    const unsigned paint_end_offset =
        ClampOffset(marker_end_offset, text_fragment);
    if (paint_start_offset == paint_end_offset)
      continue;

    switch (marker->GetType()) {
      case DocumentMarker::kSpelling:
      case DocumentMarker::kGrammar: {
        if (context.Printing())
          break;
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground)
          continue;
        DocumentMarkerPainter::PaintDocumentMarker(
            context, box_origin, style, marker->GetType(),
            text_fragment.LocalRect(paint_start_offset, paint_end_offset)
                .ToLayoutRect());
      } break;

      case DocumentMarker::kTextMatch: {
        if (!text_fragment.GetNode()
                 ->GetDocument()
                 .GetFrame()
                 ->GetEditor()
                 .MarkedTextMatchesAreHighlighted())
          break;
        const TextMatchMarker& text_match_marker =
            ToTextMatchMarkerOrDie(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          const Color color =
              LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
                  text_match_marker.IsActiveMatch());
          PaintRect(
              context, NGPhysicalOffset(box_origin),
              text_fragment.LocalRect(paint_start_offset, paint_end_offset),
              color);
          break;
        }

        const TextPaintStyle text_style =
            DocumentMarkerPainter::ComputeTextPaintStyleFrom(style,
                                                             text_match_marker);
        if (text_style.current_color == Color::kTransparent)
          break;
        text_painter->Paint(paint_start_offset, paint_end_offset,
                            paint_end_offset - paint_start_offset, text_style);
      } break;

      case DocumentMarker::kComposition:
      case DocumentMarker::kActiveSuggestion:
      case DocumentMarker::kSuggestion: {
        const StyleableMarker& styleable_marker = ToStyleableMarker(*marker);
        if (marker_paint_phase == DocumentMarkerPaintPhase::kBackground) {
          PaintRect(
              context, NGPhysicalOffset(box_origin),
              text_fragment.LocalRect(paint_start_offset, paint_end_offset),
              styleable_marker.BackgroundColor());
          break;
        }
        // TODO(yoichio) We call this on vertical/horizontal flipped context.
        // Since NGPhysicalTextFragment::LocalRect returns physical rect,
        // we need to adapt it.
        PaintStyleableMarkerUnderline(
            context, box_origin, styleable_marker, style,
            text_fragment.LocalRect(paint_start_offset, paint_end_offset));
      } break;

      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace

NGTextFragmentPainter::NGTextFragmentPainter(
    const NGPaintFragment& text_fragment)
    : fragment_(text_fragment) {
  DCHECK(text_fragment.PhysicalFragment().IsText());
}

// Logic is copied from InlineTextBoxPainter::PaintSelection.
// |selection_start| and |selection_end| should be between
// [text_fragment.StartOffset(), text_fragment.EndOffset()].
static void PaintSelection(GraphicsContext& context,
                           const NGPaintFragment& paint_fragment,
                           const Document& document,
                           const ComputedStyle& style,
                           Color text_color,
                           const LayoutRect& box_rect,
                           const LayoutSelectionStatus& selection_status) {
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragment(paint_fragment.PhysicalFragment());
  const Color color =
      SelectionBackgroundColor(document, style, text_fragment, text_color);
  const NGPhysicalOffsetRect selection_rect =
      paint_fragment.ComputeLocalSelectionRectForText(selection_status);
  PaintRect(context, NGPhysicalOffset(box_rect.Location()), selection_rect,
            color);
}

// This is copied from InlineTextBoxPainter::PaintSelection() but lacks of
// ltr, expanding new line wrap or so which uses InlineTextBox functions.
void NGTextFragmentPainter::Paint(const PaintInfo& paint_info,
                                  const LayoutPoint& paint_offset) {
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragment(fragment_.PhysicalFragment());
  const ComputedStyle& style = fragment_.Style();
  const Document& document = fragment_.GetLayoutObject()->GetDocument();

  if (!ShouldPaintTextFragment(text_fragment, style))
    return;

  NGPhysicalSize size_;
  NGPhysicalOffset offset_;

  // We round the y-axis to ensure consistent line heights.
  LayoutPoint adjusted_paint_offset =
      LayoutPoint(paint_offset.X(), LayoutUnit(paint_offset.Y().Round()));

  NGPhysicalOffset offset = fragment_.Offset();
  LayoutPoint box_origin(offset.left, offset.top);
  box_origin.Move(adjusted_paint_offset.X(), adjusted_paint_offset.Y());

  GraphicsContext& context = paint_info.context;

  bool is_printing = paint_info.IsPrinting();

  // Determine whether or not we're selected.
  const LayoutSelectionStatus& selection_status =
      document.GetFrame()->Selection().ComputeLayoutSelectionStatus(fragment_);
  DCHECK_LE(selection_status.start, selection_status.end);
  const bool have_selection = selection_status.start < selection_status.end;
  if (!have_selection && paint_info.phase == PaintPhase::kSelection) {
    // When only painting the selection, don't bother to paint if there is none.
    return;
  }

  // Determine text colors.
  TextPaintStyle text_style =
      TextPainterBase::TextPaintingStyle(document, style, paint_info);
  TextPaintStyle selection_style = TextPainterBase::SelectionPaintingStyle(
      document, style, fragment_.GetNode(), have_selection, paint_info,
      text_style);
  bool paint_selected_text_only = (paint_info.phase == PaintPhase::kSelection);
  bool paint_selected_text_separately =
      !paint_selected_text_only && text_style != selection_style;

  // Set our font.
  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);

  LayoutRect box_rect(box_origin, fragment_.Size().ToLayoutSize());
  base::Optional<GraphicsContextStateSaver> state_saver;

  // 1. Paint backgrounds behind text if needed. Examples of such backgrounds
  // include selection and composition highlights.
  // Since NGPaintFragment::ComputeLocalSelectionRectForText() returns
  // NGPhysicalOffsetRect rather than NGLogicalRect, we should paint selection
  // before GraphicsContext flip.
  // TODO(yoichio): Make NGPhysicalTextFragment::LocalRect and
  // NGPaintFragment::ComputeLocalSelectionRectForText logical so that we can
  // paint selection in same fliped dimention as NGTextPainter.
  const DocumentMarkerVector& markers_to_paint =
      ComputeMarkersToPaint(fragment_);
  if (paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip && !is_printing) {
    PaintDocumentMarkers(context, fragment_, markers_to_paint, box_origin,
                         style, DocumentMarkerPaintPhase::kBackground, nullptr);

    if (have_selection) {
      PaintSelection(context, fragment_, document, style,
                     selection_style.fill_color, box_rect, selection_status);
    }
  }

  // Line break needs only selection painting.
  if (text_fragment.IsLineBreak())
    return;

  const NGLineOrientation orientation = text_fragment.LineOrientation();
  if (orientation != NGLineOrientation::kHorizontal) {
    state_saver.emplace(context);
    // Because we rotate the GraphicsContext to be logical, flip the
    // |box_rect| to match to it.
    box_rect.SetSize(
        LayoutSize(fragment_.Size().height, fragment_.Size().width));
    context.ConcatCTM(TextPainterBase::Rotation(
        box_rect, orientation == NGLineOrientation::kClockWiseVertical
                      ? TextPainterBase::kClockwise
                      : TextPainterBase::kCounterclockwise));
  }

  // 2. Now paint the foreground, including text and decorations.
  int ascent = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  LayoutPoint text_origin(box_origin.X(), box_origin.Y() + ascent);
  NGTextPainter text_painter(context, font, text_fragment, text_origin,
                             box_rect, text_fragment.IsHorizontal());

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    text_painter.SetEmphasisMark(style.TextEmphasisMarkString(),
                                 style.GetTextEmphasisPosition());
  }

  unsigned length = text_fragment.Text().length();
  if (!paint_selected_text_only) {
    // Paint text decorations except line-through.
    DecorationInfo decoration_info;
    bool has_line_through_decoration = false;
    if (style.TextDecorationsInEffect() != TextDecoration::kNone) {
      LayoutPoint local_origin = LayoutPoint(box_origin);
      LayoutUnit width = box_rect.Width();
      const NGPhysicalBoxFragment* decorating_box = nullptr;
      const ComputedStyle* decorating_box_style =
          decorating_box ? &decorating_box->Style() : nullptr;

      text_painter.ComputeDecorationInfo(
          decoration_info, box_origin, local_origin, width,
          style.GetFontBaseline(), style, decorating_box_style);

      NGTextDecorationOffset decoration_offset(*decoration_info.style,
                                               text_fragment, decorating_box);
      text_painter.PaintDecorationsExceptLineThrough(
          decoration_offset, decoration_info, paint_info,
          style.AppliedTextDecorations(), text_style,
          &has_line_through_decoration);
    }

    unsigned start_offset = text_fragment.StartOffset();
    unsigned end_offset = start_offset + length;

    if (have_selection && paint_selected_text_separately) {
      // Paint only the text that is not selected.
      if (start_offset < selection_status.start) {
        text_painter.Paint(start_offset, selection_status.start, length,
                           text_style);
      }
      if (selection_status.end < end_offset) {
        text_painter.Paint(selection_status.end, end_offset, length,
                           text_style);
      }
    } else {
      text_painter.Paint(start_offset, end_offset, length, text_style);
    }

    // Paint line-through decoration if needed.
    if (has_line_through_decoration) {
      text_painter.PaintDecorationsOnlyLineThrough(
          decoration_info, paint_info, style.AppliedTextDecorations(),
          text_style);
    }
  }

  if (have_selection &&
      (paint_selected_text_only || paint_selected_text_separately)) {
    // Paint only the text that is selected.
    text_painter.Paint(selection_status.start, selection_status.end, length,
                       selection_style);
  }

  if (paint_info.phase != PaintPhase::kForeground)
    return;
  PaintDocumentMarkers(context, fragment_, markers_to_paint, box_origin, style,
                       DocumentMarkerPaintPhase::kForeground, &text_painter);
}

}  // namespace blink
