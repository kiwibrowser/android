// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_item.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_view_controller_presentation_delegate.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kViewControllerWidth = 312.;
const CGFloat kViewControllerHeight = 230.;
// Header height for identity section.
const CGFloat kHeaderHeight = 49.;
// Row height.
const CGFloat kRowHeight = 54.;
// Footer height for "Add Account…" section.
const CGFloat kFooterHeight = 17.;
}  // namespace

@interface IdentityChooserViewController ()

@property(nonatomic, strong)
    MDCDialogTransitionController* transitionController;

@end

@implementation IdentityChooserViewController

@synthesize presentationDelegate = _presentationDelegate;
@synthesize transitionController = _transitionController;

- (instancetype)init {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    self.modalPresentationStyle = UIModalPresentationCustom;
    _transitionController = [[MDCDialogTransitionController alloc] init];
    self.transitioningDelegate = _transitionController;
    self.modalPresentationStyle = UIModalPresentationCustom;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.preferredContentSize =
      CGSizeMake(kViewControllerWidth, kViewControllerHeight);
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.contentInset = UIEdgeInsetsMake(0, 0, kFooterHeight, 0);
  self.tableView.sectionFooterHeight = 0;
  // Setting -UITableView.rowHeight is required for iOS 10. On iOS 11, the row
  // height is automatically set.
  self.tableView.rowHeight = kRowHeight;
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.presentationDelegate identityChooserViewControllerDidDisappear:self];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return (section == 0) ? kHeaderHeight : 0;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  switch (indexPath.section) {
    case 0: {
      ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
      IdentityChooserItem* identityChooserItem =
          base::mac::ObjCCastStrict<IdentityChooserItem>(item);
      DCHECK(identityChooserItem);
      [self.presentationDelegate
          identityChooserViewController:self
            didSelectIdentityWithGaiaID:identityChooserItem.gaiaID];
      break;
    }
    case 1:
      DCHECK_EQ(0, indexPath.row);
      [self.presentationDelegate
          identityChooserViewControllerDidTapOnAddAccount:self];
      break;
    default:
      NOTREACHED();
      break;
  }
}

@end
