// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#import "ios/chrome/browser/autofill/form_suggestion_view_client.h"
#include "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Vertical margin between suggestions and the edge of the suggestion content
// frame.
const CGFloat kSuggestionVerticalMargin = 6;

// Horizontal margin around suggestions (i.e. between suggestions, and between
// the end suggestions and the suggestion content frame).
const CGFloat kSuggestionHorizontalMargin = 6;

}  // namespace

@interface FormSuggestionView ()

// Creates and adds subviews.
- (void)setupSubviews;

@end

@implementation FormSuggestionView {
  // The FormSuggestions that are displayed by this view.
  NSArray* _suggestions;

  // Handles user interactions.
  id<FormSuggestionViewClient> _client;
}

- (instancetype)initWithFrame:(CGRect)frame
                       client:(id<FormSuggestionViewClient>)client
                  suggestions:(NSArray*)suggestions {
  self = [super initWithFrame:frame];
  if (self) {
    _client = client;
    _suggestions = [suggestions copy];
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && self.subviews.count == 0) {
    [self setupSubviews];
  }
  [super willMoveToSuperview:newSuperview];
}

#pragma mark - Helper methods

- (void)setupSubviews {
  self.showsVerticalScrollIndicator = NO;
  self.showsHorizontalScrollIndicator = NO;
  self.bounces = NO;
  self.canCancelContentTouches = YES;

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.layoutMarginsRelativeArrangement = YES;
  stackView.layoutMargins =
      UIEdgeInsetsMake(kSuggestionVerticalMargin, kSuggestionHorizontalMargin,
                       kSuggestionVerticalMargin, kSuggestionHorizontalMargin);
  stackView.spacing = kSuggestionHorizontalMargin;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:stackView];
  AddSameConstraints(stackView, self);
  [stackView.heightAnchor constraintEqualToAnchor:self.heightAnchor].active =
      true;

  // Rotate the UIScrollView and its UIStackView subview 180 degrees so that the
  // first suggestion actually shows up first.
  if (base::i18n::IsRTL()) {
    self.transform = CGAffineTransformMakeRotation(M_PI);
    stackView.transform = CGAffineTransformMakeRotation(M_PI);
  }

  auto setupBlock = ^(FormSuggestion* suggestion, NSUInteger idx, BOOL* stop) {
    // Disable user interaction with suggestion if it is Google Pay logo.
    BOOL userInteractionEnabled =
        suggestion.identifier != autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING;

    UIView* label =
        [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                  index:idx
                                 userInteractionEnabled:userInteractionEnabled
                                         numSuggestions:[_suggestions count]
                                                 client:_client];
    [stackView addArrangedSubview:label];

    // If first suggestion is Google Pay logo animate it below the fold.
    if (idx == 0U &&
        suggestion.identifier == autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING) {
      const CGFloat firstLabelWidth =
          [label systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
              .width +
          kSuggestionHorizontalMargin;
      dispatch_time_t popTime =
          dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC);
      __weak FormSuggestionView* weakSelf = self;
      dispatch_after(popTime, dispatch_get_main_queue(), ^{
        [weakSelf setContentOffset:CGPointMake(firstLabelWidth,
                                               weakSelf.contentOffset.y)
                          animated:YES];
      });
    }
  };
  [_suggestions enumerateObjectsUsingBlock:setupBlock];
}

- (NSArray*)suggestions {
  return _suggestions;
}

@end
