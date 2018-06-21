// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid/grid_layout.h"

#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GridLayout ()
@property(nonatomic, strong) NSArray<NSIndexPath*>* indexPathsOfDeletingItems;
@property(nonatomic, strong) NSArray<NSIndexPath*>* indexPathsOfInsertingItems;
@end

@implementation GridLayout
@synthesize indexPathsOfDeletingItems = _indexPathsOfDeletingItems;
@synthesize indexPathsOfInsertingItems = _indexPathsOfInsertingItems;

#pragma mark - UICollectionViewLayout

// This is called whenever the layout is invalidated, including during rotation.
// Resizes item, margins, and spacing to fit new size classes and width.
- (void)prepareLayout {
  [super prepareLayout];

  UIUserInterfaceSizeClass horizontalSizeClass =
      self.collectionView.traitCollection.horizontalSizeClass;
  UIUserInterfaceSizeClass verticalSizeClass =
      self.collectionView.traitCollection.verticalSizeClass;
  CGFloat width = CGRectGetWidth(self.collectionView.bounds);
  if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    self.itemSize = kGridCellSizeSmall;
    if (width < kGridLayoutCompactCompactLimitedWidth) {
      self.sectionInset = kGridLayoutInsetsCompactCompactLimitedWidth;
      self.minimumLineSpacing =
          kGridLayoutLineSpacingCompactCompactLimitedWidth;
    } else {
      self.sectionInset = kGridLayoutInsetsCompactCompact;
      self.minimumLineSpacing = kGridLayoutLineSpacingCompactCompact;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
             verticalSizeClass == UIUserInterfaceSizeClassRegular) {
    if (width < kGridLayoutCompactRegularLimitedWidth) {
      self.itemSize = kGridCellSizeSmall;
      self.sectionInset = kGridLayoutInsetsCompactRegularLimitedWidth;
      self.minimumLineSpacing =
          kGridLayoutLineSpacingCompactRegularLimitedWidth;
    } else {
      self.itemSize = kGridCellSizeMedium;
      self.sectionInset = kGridLayoutInsetsCompactRegular;
      self.minimumLineSpacing = kGridLayoutLineSpacingCompactRegular;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassRegular &&
             verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    self.itemSize = kGridCellSizeSmall;
    self.sectionInset = kGridLayoutInsetsRegularCompact;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularCompact;
  } else {
    self.itemSize = kGridCellSizeLarge;
    self.sectionInset = kGridLayoutInsetsRegularRegular;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularRegular;
  }
}

- (void)prepareForCollectionViewUpdates:
    (NSArray<UICollectionViewUpdateItem*>*)updateItems {
  [super prepareForCollectionViewUpdates:updateItems];
  // Track which items in this update are explicitly being deleted or inserted.
  NSMutableArray<NSIndexPath*>* deletingItems =
      [NSMutableArray arrayWithCapacity:updateItems.count];
  NSMutableArray<NSIndexPath*>* insertingItems =
      [NSMutableArray arrayWithCapacity:updateItems.count];
  for (UICollectionViewUpdateItem* item in updateItems) {
    switch (item.updateAction) {
      case UICollectionUpdateActionDelete:
        [deletingItems addObject:item.indexPathBeforeUpdate];
        break;
      case UICollectionUpdateActionInsert:
        [insertingItems addObject:item.indexPathAfterUpdate];
        break;
      default:
        break;
    }
  }
  self.indexPathsOfDeletingItems = [deletingItems copy];
  self.indexPathsOfInsertingItems = [insertingItems copy];
}

- (UICollectionViewLayoutAttributes*)
finalLayoutAttributesForDisappearingItemAtIndexPath:
    (NSIndexPath*)itemIndexPath {
  // Note that this method is called for any item whose index path changing from
  // |itemIndexPath|, which includes any items that were in the layout and whose
  // index path is changing. For an item whose index path is changing, this
  // method is called before
  // -initialLayoutAttributesForAppearingItemAtIndexPath:
  UICollectionViewLayoutAttributes* attributes = [[super
      finalLayoutAttributesForDisappearingItemAtIndexPath:itemIndexPath] copy];
  // Disappearing items that aren't being deleted just use the default
  // attributes.
  if (![self.indexPathsOfDeletingItems containsObject:itemIndexPath]) {
    return attributes;
  }
  // Cells being deleted scale to 0, and are z-positioned behind all others.
  // (Note that setting the zIndex here actually has no effect, despite what is
  // implied in the UIKit documentation).
  attributes.zIndex = -10;
  // Scaled down to 0% (or near enough).
  CGAffineTransform transform =
      CGAffineTransformScale(attributes.transform, /*sx=*/0.01, /*sy=*/0.01);
  attributes.transform = transform;
  // Fade out.
  attributes.alpha = 0.0;
  return attributes;
}

- (UICollectionViewLayoutAttributes*)
initialLayoutAttributesForAppearingItemAtIndexPath:(NSIndexPath*)itemIndexPath {
  // Note that this method is called for any item whose index path is becoming
  // |itemIndexPath|, which includes any items that were in the layout but whose
  // index path is changing. For an item whose index path is changing, this
  // method is called after
  // -finalLayoutAttributesForDisappearingItemAtIndexPath:
  UICollectionViewLayoutAttributes* attributes = [[super
      initialLayoutAttributesForAppearingItemAtIndexPath:itemIndexPath] copy];
  // Appearing items that aren't being inserted just use the default
  // attributes.
  if (![self.indexPathsOfInsertingItems containsObject:itemIndexPath]) {
    return attributes;
  }
  // TODO(crbug.com/820410) : Polish the animation, and put constants where they
  // belong.
  // Cells being inserted start faded out, scaled down, and drop downwards
  // slightly.
  attributes.alpha = 0.0;
  CGAffineTransform transform =
      CGAffineTransformScale(attributes.transform, /*sx=*/0.9, /*sy=*/0.9);
  transform = CGAffineTransformTranslate(transform, /*tx=*/0,
                                         /*ty=*/attributes.size.height * 0.1);
  attributes.transform = transform;
  return attributes;
}

- (void)finalizeCollectionViewUpdates {
  self.indexPathsOfDeletingItems = @[];
  self.indexPathsOfInsertingItems = @[];
}

@end
