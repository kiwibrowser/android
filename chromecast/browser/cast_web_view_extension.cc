// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_extension.h"

#include "base/logging.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_extension_host.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_system.h"
#include "net/base/net_errors.h"

namespace chromecast {

CastWebViewExtension::CastWebViewExtension(
    const CreateParams& params,
    content::BrowserContext* browser_context,
    scoped_refptr<content::SiteInstance> site_instance,
    const extensions::Extension* extension,
    const GURL& initial_url)
    : delegate_(params.delegate),
      window_(shell::CastContentWindow::Create(params.delegate,
                                               params.is_headless,
                                               params.enable_touch_input)),
      extension_host_(std::make_unique<CastExtensionHost>(
          browser_context,
          params.delegate,
          extension,
          initial_url,
          site_instance.get(),
          extensions::VIEW_TYPE_EXTENSION_POPUP)),
      remote_debugging_server_(
          shell::CastBrowserProcess::GetInstance()->remote_debugging_server()) {
  DCHECK(delegate_);
  content::WebContentsObserver::Observe(web_contents());
  // If this CastWebView is enabled for development, start the remote debugger.
  if (params.enabled_for_dev) {
    LOG(INFO) << "Enabling dev console for " << web_contents()->GetVisibleURL();
    remote_debugging_server_->EnableWebContentsForDebugging(web_contents());
  }
}

CastWebViewExtension::~CastWebViewExtension() {
  content::WebContentsObserver::Observe(nullptr);
}

shell::CastContentWindow* CastWebViewExtension::window() const {
  return window_.get();
}

content::WebContents* CastWebViewExtension::web_contents() const {
  return extension_host_->host_contents();
}

void CastWebViewExtension::LoadUrl(GURL url) {
  extension_host_->CreateRenderViewSoon();
}

void CastWebViewExtension::ClosePage(const base::TimeDelta& shutdown_delay) {}

void CastWebViewExtension::InitializeWindow(
    CastWindowManager* window_manager,
    bool is_visible,
    CastWindowManager::WindowId z_order,
    VisibilityPriority initial_priority) {
  window_->CreateWindowForWebContents(web_contents(), window_manager,
                                      is_visible, z_order, initial_priority);
  web_contents()->Focus();
}

void CastWebViewExtension::WebContentsDestroyed() {
  delegate_->OnPageStopped(net::OK);
}

void CastWebViewExtension::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (view) {
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
}

void CastWebViewExtension::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the navigation was not committed, it means either the page was a
  // download or error 204/205, or the navigation never left the previous
  // URL. Basically, this isn't a problem since we stayed at the existing URL.
  if (!navigation_handle->HasCommitted())
    return;

  // The navigation committed. If we navigated to an error page then
  // this is a bad state, and should be notified. Otherwise the navigation
  // completed as intended.
  if (!navigation_handle->IsErrorPage())
    return;

  net::Error error_code = navigation_handle->GetNetErrorCode();
  // Ignore sub-frame errors.
  if (!navigation_handle->IsInMainFrame()) {
    LOG(ERROR) << "Got error on sub-frame: url=" << navigation_handle->GetURL()
               << ", error=" << error_code
               << ", description=" << net::ErrorToShortString(error_code);
    return;
  }

  LOG(ERROR) << "Got error on navigation: url=" << navigation_handle->GetURL()
             << ", error_code=" << error_code
             << ", description= " << net::ErrorToShortString(error_code);
  delegate_->OnPageStopped(error_code);
}

void CastWebViewExtension::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  // Only report an error if we are the main frame.
  if (render_frame_host->GetParent()) {
    LOG(ERROR) << "Got error on sub-frame: url=" << validated_url.spec()
               << ", error=" << error_code << ": " << error_description;
    return;
  }
  LOG(ERROR) << "Got error on load: url=" << validated_url.spec()
             << ", error_code=" << error_code << ": " << error_description;
  ;
  delegate_->OnPageStopped(error_code);
}

void CastWebViewExtension::RenderProcessGone(base::TerminationStatus status) {
  delegate_->OnPageStopped(net::ERR_UNEXPECTED);
}

}  // namespace chromecast
