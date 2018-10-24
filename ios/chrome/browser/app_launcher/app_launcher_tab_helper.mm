// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"

#import <UIKit/UIKit.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/web/app_launcher_abuse_detector.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_client.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(AppLauncherTabHelper);

// This enum used by the Applauncher to log to UMA, if App launching request was
// allowed or blocked.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExternalURLRequestStatus {
  kMainFrameRequestAllowed = 0,
  kSubFrameRequestAllowed = 1,
  kSubFrameRequestBlocked = 2,
  kCount,
};

void AppLauncherTabHelper::CreateForWebState(
    web::WebState* web_state,
    AppLauncherAbuseDetector* abuse_detector,
    id<AppLauncherTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new AppLauncherTabHelper(
                               web_state, abuse_detector, delegate)));
  }
}

AppLauncherTabHelper::AppLauncherTabHelper(
    web::WebState* web_state,
    AppLauncherAbuseDetector* abuse_detector,
    id<AppLauncherTabHelperDelegate> delegate)
    : web::WebStatePolicyDecider(web_state),
      abuse_detector_(abuse_detector),
      delegate_(delegate),
      weak_factory_(this) {}

AppLauncherTabHelper::~AppLauncherTabHelper() = default;

bool AppLauncherTabHelper::RequestToLaunchApp(const GURL& url,
                                              const GURL& source_page_url,
                                              bool link_tapped) {
  if (!url.is_valid() || !url.has_scheme())
    return false;

  // Don't open external application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    return false;
  }

  // Don't try to open external application if a prompt is already active.
  if (is_prompt_active_)
    return false;

  [abuse_detector_ didRequestLaunchExternalAppURL:url
                                fromSourcePageURL:source_page_url];
  ExternalAppLaunchPolicy policy =
      [abuse_detector_ launchPolicyForURL:url
                        fromSourcePageURL:source_page_url];
  switch (policy) {
    case ExternalAppLaunchPolicyBlock: {
      return false;
    }
    case ExternalAppLaunchPolicyAllow: {
      return [delegate_ appLauncherTabHelper:this
                            launchAppWithURL:url
                                  linkTapped:link_tapped];
    }
    case ExternalAppLaunchPolicyPrompt: {
      is_prompt_active_ = true;
      base::WeakPtr<AppLauncherTabHelper> weak_this =
          weak_factory_.GetWeakPtr();
      GURL copied_url = url;
      GURL copied_source_page_url = source_page_url;
      [delegate_ appLauncherTabHelper:this
          showAlertOfRepeatedLaunchesWithCompletionHandler:^(
              BOOL user_allowed) {
            if (!weak_this.get())
              return;
            if (user_allowed) {
              // By confirming that user wants to launch the application, there
              // is no need to check for |link_tapped|.
              [delegate_ appLauncherTabHelper:weak_this.get()
                             launchAppWithURL:copied_url
                                   linkTapped:YES];
            } else {
              // TODO(crbug.com/674649): Once non modal dialogs are implemented,
              // update this to always prompt instead of blocking the app.
              [abuse_detector_ blockLaunchingAppURL:copied_url
                                  fromSourcePageURL:copied_source_page_url];
            }
            is_prompt_active_ = false;
          }];
      return true;
    }
  }
}

bool AppLauncherTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  GURL requestURL = net::GURLWithNSURL(request.URL);
  if (web::UrlHasWebScheme(requestURL) ||
      web::GetWebClient()->IsAppSpecificURL(requestURL) ||
      requestURL.SchemeIs(url::kFileScheme) ||
      requestURL.SchemeIs(url::kAboutScheme)) {
    // This URL can be handled by the WebState and doesn't require App launcher
    // handling.
    return true;
  }

  ExternalURLRequestStatus request_status = ExternalURLRequestStatus::kCount;

  if (request_info.target_frame_is_main) {
    // TODO(crbug.com/852489): Check if the source frame should also be
    // considered.
    request_status = ExternalURLRequestStatus::kMainFrameRequestAllowed;
  } else {
    request_status = request_info.has_user_gesture
                         ? ExternalURLRequestStatus::kSubFrameRequestAllowed
                         : ExternalURLRequestStatus::kSubFrameRequestBlocked;
  }

  DCHECK_NE(request_status, ExternalURLRequestStatus::kCount);
  UMA_HISTOGRAM_ENUMERATION("WebController.ExternalURLRequestBlocking",
                            request_status, ExternalURLRequestStatus::kCount);

  return request_status != ExternalURLRequestStatus::kSubFrameRequestBlocked;
}
