// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/browser/cast_web_view.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class SiteInstance;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromecast {

class CastWebContentsManager;

struct ActiveWebview {
  CastWebView* web_view;
  int id;
};

class CastWebViewFactory : public CastWebView::Observer {
 public:
  explicit CastWebViewFactory(content::BrowserContext* browser_context);
  ~CastWebViewFactory() override;

  virtual std::unique_ptr<CastWebView> CreateWebView(
      const CastWebView::CreateParams& params,
      CastWebContentsManager* web_contents_manager,
      scoped_refptr<content::SiteInstance> site_instance,
      const extensions::Extension* extension,
      const GURL& initial_url);

  const std::vector<ActiveWebview>& active_webviews() const {
    return active_webviews_;
  }
  content::BrowserContext* browser_context() const { return browser_context_; }

 protected:
  // CastWebView::Observer implementation:
  void OnPageDestroyed(CastWebView* web_view) override;

  content::BrowserContext* const browser_context_;
  base::RepeatingCallback<void(CastWebView*, int)> register_callback_;

  std::vector<ActiveWebview> active_webviews_;
  int next_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(CastWebViewFactory);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_FACTORY_H_
