// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_UI_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_UI_H_

#import <UIKit/UIKit.h>

// Protocol for the UI displaying the toolbar.
@protocol ToolbarUI<NSObject>

// The minimum height of the toolbar relative to the browser content area.
// This should be broadcast using |-broadcastCollapsedToolbarHeight:|.
@property(nonatomic, readonly) CGFloat collapsedHeight;

// The minimum height of the toolbar relative to the browser content area.
// This should be broadcast using |-broadcastExpandedToolbarHeight:|.
@property(nonatomic, readonly) CGFloat expandedHeight;

@end

// Simple implementation of ToolbarUI that allows readwrite access to broadcast
// properties.
@interface ToolbarUIState : NSObject<ToolbarUI>

// Redefine properties as readwrite.
@property(nonatomic, assign) CGFloat collapsedHeight;
@property(nonatomic, assign) CGFloat expandedHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_UI_H_
