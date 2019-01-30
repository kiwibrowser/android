// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_text_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "extensions/common/image_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace {

// The left-hand margin used for rows with the refresh UI.
static constexpr int kRefreshMarginLeft = 4;

// In the MD refresh or rich suggestions, x-offset of the content and
// description text.
static constexpr int kTextIndent = 48;

// TODO(dschuyler): Perhaps this should be based on the font size
// instead of hardcoded to 2 dp (e.g. by adding a space in an
// appropriate font to the beginning of the description, then reducing
// the additional padding here to zero).
static constexpr int kAnswerIconToTextPadding = 2;

// The edge length of the refresh layout image area.
static constexpr int kRefreshImageBoxSize = 40;

// The diameter of the new answer layout images.
static constexpr int kNewAnswerImageSize = 24;

// The edge length of the entity suggestions images.
static constexpr int kEntityImageSize = 32;
static constexpr int kEntityImageCornerRadius = 4;

// The minimum vertical margin that should be used above and below each
// suggestion.
static constexpr int kMinVerticalMargin = 1;

// The margin height of a split row when MD refresh is enabled.
static constexpr int kRefreshSplitMarginHeight = 8;

// The margin height of a rich suggestion row.
static constexpr int kRichSuggestionMarginHeight = 4;

// Returns the padding width between elements.
int HorizontalPadding() {
  return GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING) +
         GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING).width() / 2;
}

// Returns the horizontal offset that ensures icons align vertically with the
// Omnibox icon.  The alignment offset (labeled "a" in the diagram below) and
// padding (p) are used thusly:
//
//     +---+---+------+---+-------------------------------+----+
//     | a | p | icon | p | "result text"                 | p* |
//     +---+---+------+---+-------------------------------+----+
//
// I.e. the icon alignment offset is only used on the starting edge as a
// workaround to get the text input bar and the drop down contents to line up.
//
// *The last padding is not present.
// TODO(dschuyler): add end margin/padding.
int GetIconAlignmentOffset() {
  // The horizontal bounds of a result is the width of the selection highlight
  // (i.e. the views::Background). The traditional popup is designed with its
  // selection shape mimicking the internal shape of the omnibox border. Inset
  // to be consistent with the border drawn in BackgroundWith1PxBorder.
  int offset = LocationBarView::GetBorderThicknessDip();

  // The touch-optimized popup selection always fills the results frame. So to
  // align icons, inset additionally by the frame alignment inset on the left.
  if (ui::MaterialDesignController::IsTouchOptimizedUiEnabled()) {
    offset +=
        RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().left();
  }
  return offset;
}

// Returns the margins that should appear around the result.
// |is_two_line| indicates whether the vertical margin is for a omnibox
// result displaying an answer to the query.
gfx::Insets GetMarginInsets(int text_height, bool is_two_line) {
  // TODO(dschuyler): Consider adding a right-hand margin to each |return|.
  if (ui::MaterialDesignController::IsRefreshUi()) {
    return is_two_line
               ? gfx::Insets(kRichSuggestionMarginHeight, kRefreshMarginLeft,
                             kRichSuggestionMarginHeight, 0)
               : gfx::Insets(kRefreshSplitMarginHeight, kRefreshMarginLeft,
                             kRefreshSplitMarginHeight, 0);
  }

  // Regardless of the text size, we ensure a minimum size for the content line
  // here. This minimum is larger for hybrid mouse/touch devices to ensure an
  // adequately sized touch target.
  const int min_height_for_icon =
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE) +
      (OmniboxFieldTrial::GetSuggestionVerticalMargin() * 2);
  const int min_height_for_text = text_height + 2 * kMinVerticalMargin;
  int min_height = std::max(min_height_for_icon, min_height_for_text);

  int alignment_offset = GetIconAlignmentOffset();
  // Make sure the minimum height of an omnibox result matches the height of the
  // location bar view / non-results section of the omnibox popup in touch.
  if (ui::MaterialDesignController::IsTouchOptimizedUiEnabled()) {
    min_height = std::max(
        min_height, RoundedOmniboxResultsFrame::GetNonResultSectionHeight());
    if (is_two_line) {
      // Two-line layouts apply the normal margin at the top and the minimum
      // allowable margin at the bottom.
      const int top_margin = gfx::ToCeiledInt((min_height - text_height) / 2.f);
      return gfx::Insets(top_margin, alignment_offset + HorizontalPadding(),
                         kMinVerticalMargin, 0);
    }
  }

  const int total_margin = min_height - text_height;
  // Ceiling the top margin to account for |total_margin| being an odd number.
  const int top_margin = gfx::ToCeiledInt(total_margin / 2.f);
  const int bottom_margin = total_margin - top_margin;
  return gfx::Insets(top_margin, alignment_offset + HorizontalPadding(),
                     bottom_margin, 0);
}

////////////////////////////////////////////////////////////////////////////////
// PlaceholderImageSource:

class PlaceholderImageSource : public gfx::CanvasImageSource {
 public:
  PlaceholderImageSource(const gfx::Size& canvas_size, SkColor color);
  ~PlaceholderImageSource() override;

  // CanvasImageSource override:
  void Draw(gfx::Canvas* canvas) override;

 private:
  SkColor color_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(PlaceholderImageSource);
};

PlaceholderImageSource::PlaceholderImageSource(const gfx::Size& canvas_size,
                                               SkColor color)
    : gfx::CanvasImageSource(canvas_size, false),
      color_(color),
      size_(canvas_size) {}

PlaceholderImageSource::~PlaceholderImageSource() = default;

void PlaceholderImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setColor(color_);
  canvas->sk_canvas()->drawRoundRect(gfx::RectToSkRect(gfx::Rect(size_)),
                                     kEntityImageCornerRadius,
                                     kEntityImageCornerRadius, flags);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImageView:

class OmniboxImageView : public views::ImageView {
 public:
  OmniboxImageView() = default;

  bool CanProcessEventsWithinSubtree() const override { return false; }

 private:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;

  DISALLOW_COPY_AND_ASSIGN(OmniboxImageView);
};

void OmniboxImageView::OnPaint(gfx::Canvas* canvas) {
  gfx::Path mask;
  mask.addRoundRect(gfx::RectToSkRect(GetImageBounds()),
                    kEntityImageCornerRadius, kEntityImageCornerRadius);
  canvas->ClipPath(mask, true);
  ImageView::OnPaint(canvas);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OmniboxMatchCellView:

OmniboxMatchCellView::OmniboxMatchCellView(OmniboxResultView* result_view)
    : is_old_style_answer_(false),
      is_rich_suggestion_(false),
      is_search_type_(false) {
  AddChildView(icon_view_ = new OmniboxImageView());
  AddChildView(image_view_ = new OmniboxImageView());
  AddChildView(content_view_ = new OmniboxTextView(result_view));
  AddChildView(description_view_ = new OmniboxTextView(result_view));
  AddChildView(separator_view_ = new OmniboxTextView(result_view));

  if (ui::MaterialDesignController::IsRefreshUi()) {
    icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
    icon_view_->SetVerticalAlignment(views::ImageView::CENTER);
  }
  image_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  image_view_->SetVerticalAlignment(views::ImageView::CENTER);

  const base::string16& separator =
      l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
  separator_view_->SetText(separator);
}

OmniboxMatchCellView::~OmniboxMatchCellView() = default;

gfx::Size OmniboxMatchCellView::CalculatePreferredSize() const {
  int height = 0;
  if (is_rich_suggestion_ || has_tab_match_) {
    height = content_view_->GetLineHeight() +
             description_view_->GetHeightForWidth(width() - kTextIndent);
  } else if (is_old_style_answer_) {
    int icon_width = icon_view_->width();
    int answer_image_size =
        image_view_->GetImage().isNull()
            ? 0
            : image_view_->height() + kAnswerIconToTextPadding;
    int deduction = icon_width + (HorizontalPadding() * 3) + answer_image_size;
    int description_width = std::max(width() - deduction, 0);
    height = content_view_->GetLineHeight() +
             description_view_->GetHeightForWidth(description_width);
  } else {
    height = content_view_->GetLineHeight();
  }
  height += GetInsets().height();
  // Width is not calculated because it's not needed by current callers.
  return gfx::Size(0, height);
}

bool OmniboxMatchCellView::CanProcessEventsWithinSubtree() const {
  return false;
}

int OmniboxMatchCellView::IconWidthAndPadding() const {
  return ui::MaterialDesignController::IsRefreshUi()
             ? kTextIndent
             : icon_view_->width() + (HorizontalPadding() * 2);
}

void OmniboxMatchCellView::OnMatchUpdate(const OmniboxResultView* result_view,
                                         const AutocompleteMatch& match) {
  is_old_style_answer_ = !!match.answer;
  is_rich_suggestion_ =
      (OmniboxFieldTrial::IsNewAnswerLayoutEnabled() && !!match.answer) ||
      (OmniboxFieldTrial::IsRichEntitySuggestionsEnabled() &&
       !match.image_url.empty());
  is_search_type_ = AutocompleteMatch::IsSearchType(match.type);
  has_tab_match_ = match.has_tab_match;

  // Set up the small icon.
  if (is_rich_suggestion_) {
    icon_view_->SetSize(gfx::Size());
  } else {
    icon_view_->SetSize(icon_view_->CalculatePreferredSize());
  }

  // Set up the separator.
  if (is_old_style_answer_ || is_rich_suggestion_ || has_tab_match_) {
    separator_view_->SetSize(gfx::Size());
  } else {
    separator_view_->SetSize(separator_view_->CalculatePreferredSize());
  }

  if (!is_rich_suggestion_) {
    // An entry with |is_old_style_answer_| may use the image_view_. But it's
    // set when the image arrives (later).
    image_view_->SetImage(gfx::ImageSkia());
    image_view_->SetSize(gfx::Size());
  } else {
    // Determine if we have a local icon (or else it will be downloaded).
    const gfx::VectorIcon* vector_icon = nullptr;
    int idr_image = 0;
    if (match.answer) {
      switch (match.answer->type()) {
        case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
          vector_icon = &omnibox::kAnswerCurrencyIcon;
          break;
        case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
          vector_icon = &omnibox::kAnswerDictionaryIcon;
          break;
        case SuggestionAnswer::ANSWER_TYPE_FINANCE:
          vector_icon = &omnibox::kAnswerFinanceIcon;
          break;
        case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
          vector_icon = &omnibox::kAnswerSunriseIcon;
          break;
        case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
          idr_image = IDR_OMNIBOX_TRANSLATION_ROUND;
          break;
        case SuggestionAnswer::ANSWER_TYPE_WEATHER:
          // Weather icons are downloaded. Do nothing.
          break;
        case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
          vector_icon = &omnibox::kAnswerWhenIsIcon;
          break;
        default:
          vector_icon = &omnibox::kAnswerDefaultIcon;
          break;
      }
      if (vector_icon) {
        image_view_->SetImage(gfx::CreateVectorIcon(
            *vector_icon, kNewAnswerImageSize, gfx::kGoogleBlue600));
      } else if (idr_image) {
        image_view_->SetImage(
            ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                idr_image));
      }
      // Always set the image size so that downloaded images get the correct
      // size (such as Weather answers).
      image_view_->SetImageSize(
          gfx::Size(kNewAnswerImageSize, kNewAnswerImageSize));
    } else {
      SkColor color = result_view->GetColor(OmniboxPart::RESULTS_BACKGROUND);
      extensions::image_util::ParseHexColorString(match.image_dominant_color,
                                                  &color);
      color = SkColorSetA(color, 0x40);  // 25% transparency (arbitrary).
      const gfx::Size size = gfx::Size(kEntityImageSize, kEntityImageSize);
      image_view_->SetImageSize(size);
      image_view_->SetImage(
          gfx::CanvasImageSource::MakeImageSkia<PlaceholderImageSource>(size,
                                                                        color));
    }
  }
}

const char* OmniboxMatchCellView::GetClassName() const {
  return "OmniboxMatchCellView";
}

void OmniboxMatchCellView::Layout() {
  // Update the margins.
  gfx::Insets insets = GetMarginInsets(
      content()->GetLineHeight(),
      is_rich_suggestion_ || has_tab_match_ || is_old_style_answer_);
  SetBorder(views::CreateEmptyBorder(insets.top(), insets.left(),
                                     insets.bottom(), insets.right()));
  // Layout children *after* updating the margins.
  views::View::Layout();

  const int icon_view_width = ui::MaterialDesignController::IsRefreshUi()
                                  ? kRefreshImageBoxSize
                                  : icon_view_->width();
  const int text_indent = ui::MaterialDesignController::IsRefreshUi()
                              ? kTextIndent
                              : icon_view_->width() + HorizontalPadding();

  if (is_rich_suggestion_ || has_tab_match_) {
    LayoutNewStyleTwoLineSuggestion();
  } else if (is_old_style_answer_) {
    LayoutOldStyleAnswer(icon_view_width, text_indent);
  } else {
    LayoutSplit(icon_view_width, text_indent);
  }
}

void OmniboxMatchCellView::LayoutOldStyleAnswer(int icon_view_width,
                                                int text_indent) {
  // TODO(dschuyler): Remove this layout once rich layouts are enabled by
  // default.
  gfx::Rect child_area = GetContentsBounds();
  const int text_height = content_view_->GetLineHeight();
  int x = child_area.x();
  int y = child_area.y();
  icon_view_->SetBounds(x, y, icon_view_width, text_height);
  x += text_indent;
  content_view_->SetBounds(x, y, width() - x, text_height);
  y += text_height;
  if (!image_view_->GetImage().isNull()) {
    constexpr int kImageEdgeLength = 24;
    constexpr int kImagePadding = 2;
    image_view_->SetBounds(x, y + kImagePadding, kImageEdgeLength,
                           kImageEdgeLength);
    image_view_->SetImageSize(gfx::Size(kImageEdgeLength, kImageEdgeLength));
    x += image_view_->width() + kAnswerIconToTextPadding;
  }
  int description_width = width() - x;
  description_view_->SetBounds(
      x, y, description_width,
      description_view_->GetHeightForWidth(description_width));
}

void OmniboxMatchCellView::LayoutNewStyleTwoLineSuggestion() {
  gfx::Rect child_area = GetContentsBounds();
  int x = child_area.x();
  int y = child_area.y();
  views::ImageView* image_view;
  if (is_rich_suggestion_) {
    image_view = image_view_;
  } else {
    image_view = icon_view_;
  }
  image_view->SetBounds(x, y, kRefreshImageBoxSize, child_area.height());
  const int text_width = child_area.width() - kTextIndent;
  const int text_height = content_view_->GetLineHeight();
  content_view_->SetBounds(x + kTextIndent, y, text_width, text_height);
  description_view_->SetBounds(
      x + kTextIndent, y + text_height, text_width,
      description_view_->GetHeightForWidth(text_width));
}

void OmniboxMatchCellView::LayoutSplit(int icon_view_width, int text_indent) {
  gfx::Rect child_area = GetContentsBounds();
  int row_height = child_area.height();
  int y = child_area.y();
  icon_view_->SetBounds(child_area.x(), y, icon_view_width, row_height);
  int content_width = content_view_->CalculatePreferredSize().width();
  int description_width = description_view_->CalculatePreferredSize().width();
  gfx::Size separator_size = separator_view_->CalculatePreferredSize();
  OmniboxPopupModel::ComputeMatchMaxWidths(
      content_width, separator_size.width(), description_width,
      child_area.width() - text_indent,
      /*description_on_separate_line=*/false, !is_search_type_, &content_width,
      &description_width);
  int x = child_area.x() + text_indent;
  content_view_->SetBounds(x, y, content_width, row_height);
  if (description_width != 0) {
    x += content_view_->width();
    separator_view_->SetSize(separator_size);
    separator_view_->SetBounds(x, y, separator_view_->width(), row_height);
    x += separator_view_->width();
    description_view_->SetBounds(x, y, description_width, row_height);
  } else {
    description_view_->SetSize(gfx::Size());
    separator_view_->SetSize(gfx::Size());
  }
}
