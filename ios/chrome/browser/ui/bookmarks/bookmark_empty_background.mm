// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bookmarks/bookmark_empty_background.h"

#import "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The spacing between the image and text label.
const CGFloat kStackViewSpacing = 24.0;
const CGFloat kLegacyStackViewSpacing = 5.0;

// The margin between the stack view and the edges of its superview.
const CGFloat kStackViewMargin = 24.0;

// The font size to use when UIRefresh is disabled.
const CGFloat kLegacyFontSize = 16.0;

}  // namespace

@interface BookmarkEmptyBackground ()

// Stack view containing UI that should be centered in the page.
@property(nonatomic, strong) UIStackView* stackView;

// Text label showing the description text.
@property(nonatomic, retain) UILabel* textLabel;

@end

@implementation BookmarkEmptyBackground
@synthesize stackView = _stackView;
@synthesize textLabel = _textLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    BOOL useRefreshUI = experimental_flags::IsBookmarksUIRebootEnabled();

    // The "star" image.
    NSString* imageName =
        useRefreshUI ? @"bookmark_empty_star" : @"bookmark_gray_star_large";
    UIImageView* imageView =
        [[UIImageView alloc] initWithImage:[UIImage imageNamed:imageName]];

    // The explanatory text label.
    self.textLabel = [[UILabel alloc] init];
    self.textLabel.backgroundColor = [UIColor clearColor];
    self.textLabel.accessibilityIdentifier =
        kBookmarkEmptyStateExplanatoryLabelIdentifier;
    self.textLabel.textAlignment = NSTextAlignmentCenter;
    self.textLabel.numberOfLines = 0;
    if (useRefreshUI) {
      self.textLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      self.textLabel.adjustsFontForContentSizeCategory = YES;
      self.textLabel.textColor = [UIColor colorWithWhite:0 alpha:100.0 / 255];
    } else {
      self.textLabel.font =
          [[MDCTypography fontLoader] mediumFontOfSize:kLegacyFontSize];
      self.textLabel.textColor = [UIColor colorWithWhite:0 alpha:110.0 / 255];
    }

    // Vertical stack view that centers its contents.
    _stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ imageView, self.textLabel ]];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.axis = UILayoutConstraintAxisVertical;
    _stackView.alignment = UIStackViewAlignmentCenter;
    _stackView.distribution = UIStackViewDistributionEqualSpacing;
    _stackView.spacing =
        useRefreshUI ? kStackViewSpacing : kLegacyStackViewSpacing;
    [self addSubview:_stackView];

    // Center the stack view, with a minimum margin around the leading and
    // trailing edges.
    [NSLayoutConstraint activateConstraints:@[
      [self.centerXAnchor constraintEqualToAnchor:_stackView.centerXAnchor],
      [self.centerYAnchor constraintEqualToAnchor:_stackView.centerYAnchor],
      [self.leadingAnchor
          constraintLessThanOrEqualToAnchor:_stackView.leadingAnchor
                                   constant:-kStackViewMargin],
      [self.trailingAnchor
          constraintGreaterThanOrEqualToAnchor:_stackView.trailingAnchor
                                      constant:kStackViewMargin],

    ]];
  }
  return self;
}

- (NSString*)text {
  return self.textLabel.text;
}

- (void)setText:(NSString*)text {
  self.textLabel.text = text;
  [self setNeedsLayout];
}

@end
