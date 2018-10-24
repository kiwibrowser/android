// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/favicon_loader.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_callback.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#include "skia/ext/skia_utils_ios.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FaviconLoader::FaviconLoader(favicon::LargeIconService* large_icon_service)
    : large_icon_service_(large_icon_service),
      favicon_cache_([[NSCache alloc] init]) {}
FaviconLoader::~FaviconLoader() {}

// TODO(pinkerton): How do we update the favicon if it's changed on the web?
// We can possibly just rely on this class being purged or the app being killed
// to reset it, but then how do we ensure the FaviconService is updated?
FaviconAttributes* FaviconLoader::FaviconForUrl(
    const GURL& url,
    float size,
    float min_size,
    FaviconAttributesCompletionBlock block) {
  NSString* key = base::SysUTF8ToNSString(url.spec());
  FaviconAttributes* value = [favicon_cache_ objectForKey:key];
  if (value) {
    return value;
  }

  GURL block_url(url);
  auto favicon_block = ^(const favicon_base::LargeIconResult& result) {
    // GetLargeIconOrFallbackStyle() either returns a valid favicon (which can
    // be the default favicon) or fallback attributes.
    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
      // The favicon code assumes favicons are PNG-encoded.
      UIImage* favicon =
          [UIImage imageWithData:[NSData dataWithBytes:data->front()
                                                length:data->size()]];
      FaviconAttributes* attributes =
          [FaviconAttributes attributesWithImage:favicon];
      [favicon_cache_ setObject:attributes forKey:key];
      block(attributes);
      return;
    }
    DCHECK(result.fallback_icon_style);
    FaviconAttributes* attributes = [FaviconAttributes
        attributesWithMonogram:base::SysUTF16ToNSString(
                                   favicon::GetFallbackIconText(block_url))
                     textColor:skia::UIColorFromSkColor(
                                   result.fallback_icon_style->text_color)
               backgroundColor:skia::UIColorFromSkColor(
                                   result.fallback_icon_style->background_color)
        defaultBackgroundColor:result.fallback_icon_style->
                               is_default_background_color];
    [favicon_cache_ setObject:attributes forKey:key];
    block(attributes);
  };

  CGFloat favicon_size_in_pixels = [UIScreen mainScreen].scale * size;
  CGFloat min_favicon_size = [UIScreen mainScreen].scale * min_size;
  DCHECK(large_icon_service_);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      url, min_favicon_size, favicon_size_in_pixels,
      base::BindRepeating(favicon_block), &cancelable_task_tracker_);

  if (experimental_flags::IsCollectionsUIRebootEnabled()) {
    return [FaviconAttributes
        attributesWithImage:[UIImage imageNamed:@"default_world_favicon"]];
  }
  return [FaviconAttributes
      attributesWithImage:[UIImage imageNamed:@"default_favicon"]];
}

void FaviconLoader::CancellAllRequests() {
  cancelable_task_tracker_.TryCancelAll();
}
