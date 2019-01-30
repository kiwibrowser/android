// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#import "ios/chrome/browser/ui/material_components/app_bar_presenting.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_consumer.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"

@class ChromeTableViewStyler;
@class TableViewItem;

typedef NS_ENUM(NSInteger, ChromeTableViewControllerStyle) {
  ChromeTableViewControllerStyleNoAppBar,
  ChromeTableViewControllerStyleWithAppBar,
};

// Chrome-specific TableViewController.
@interface ChromeTableViewController
    : UITableViewController<AppBarPresenting, ChromeTableViewConsumer>

// The model of this controller.
@property(nonatomic, readonly, strong)
    TableViewModel<TableViewItem*>* tableViewModel;

// The styler that controls how this table view and its cells are
// displayed. Styler changes should be made before viewDidLoad is called; any
// changes made afterwards are not guaranteed to take effect.
@property(nonatomic, readwrite, strong) ChromeTableViewStyler* styler;

// Initializes the view controller, configured with |style|, |appBarStyle|. The
// default ChromeTableViewStyler will be used.
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_DESIGNATED_INITIALIZER;
// Unavailable initializers.
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Initializes the collection view model. Must be called by subclasses if they
// override this method in order to get a clean tableViewModel.
- (void)loadModel NS_REQUIRES_SUPER;

// Adds and starts a loading indicator in the center of the
// ChromeTableViewController, if one is not already present. This will remove
// any existing table view background views.
- (void)startLoadingIndicatorWithLoadingMessage:(NSString*)loadingMessage;

// Removes and stops the loading indicator, if one is present.
- (void)stopLoadingIndicatorWithCompletion:(ProceduralBlock)completion;

// Methods for reconfiguring and reloading the table view are provided by
// ChromeTableViewConsumer.

#pragma mark - Presentation Controller integration

// Returns YES if this view controller should be dismissed when the user touches
// outside the bounds of the table view.  Defaults to YES.  Subclasses should
// override this to return NO if they allow the user to edit data, so that
// accidental touches outside the table view cannot lose user data.
- (BOOL)shouldBeDismissedOnTouchOutside;

#pragma mark - UIScrollViewDelegate

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView NS_REQUIRES_SUPER;

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate NS_REQUIRES_SUPER;

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView
    NS_REQUIRES_SUPER;

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset
    NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_
