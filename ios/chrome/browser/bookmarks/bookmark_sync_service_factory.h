// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace sync_bookmarks {
class BookmarkSyncService;
}

namespace ios {

class ChromeBrowserState;

// Singleton that owns the bookmark sync service.
class BookmarkSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of BookmarkSyncService associated with this profile
  // (creating one if none exists).
  static sync_bookmarks::BookmarkSyncService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  // Returns an instance of the BookmarkSyncServiceFactory singleton.
  static BookmarkSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<BookmarkSyncServiceFactory>;

  BookmarkSyncServiceFactory();
  ~BookmarkSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BookmarkSyncServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_SYNC_SERVICE_FACTORY_H_
