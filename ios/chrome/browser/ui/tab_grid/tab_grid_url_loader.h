// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_URL_LOADER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_URL_LOADER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/url_loader.h"

namespace ios {
class ChromeBrowserState;
}

class WebStateList;

// Specialized URLLoader for the tab grid.
// Loading a URL or sessionTab normally navigates the currently visible tab
// view, which meaning replacing the active WebState. When there is no currently
// visible tab view, loading a URL means adding a new WebState to the
// WebStateList.
@interface TabGridURLLoader : NSObject<UrlLoader>
- (instancetype)
initWithRegularWebStateList:(WebStateList*)regularWebStateList
      incognitoWebStateList:(WebStateList*)incognitoWebStateList
        regularBrowserState:(ios::ChromeBrowserState*)regularBrowserState
      incognitoBrowserState:(ios::ChromeBrowserState*)incognitoBrowserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_URL_LOADER_H_
