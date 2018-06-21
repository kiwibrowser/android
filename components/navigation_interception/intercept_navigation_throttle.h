// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
class WebContents;
}

namespace navigation_interception {

class NavigationParams;

// This class allows the provider of the Callback to selectively ignore top
// level navigations. This is a UI thread class.
class InterceptNavigationThrottle : public content::NavigationThrottle {
 public:
  typedef base::RepeatingCallback<bool(
      content::WebContents* /* source */,
      const NavigationParams& /* navigation_params */)>
      CheckCallback;

  static const base::Feature kAsyncCheck;

  InterceptNavigationThrottle(content::NavigationHandle* navigation_handle,
                              CheckCallback should_ignore_callback);
  ~InterceptNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  // To be called on either WillFailRequest or WillProcessResponse.
  ThrottleCheckResult WillFinish();

  ThrottleCheckResult CheckIfShouldIgnoreNavigation(bool is_redirect);
  void RunCheck(const NavigationParams& params);

  bool ShouldCheckAsynchronously() const;

  // Constructs NavigationParams for this navigation.
  NavigationParams GetNavigationParams(bool is_redirect) const;

  // This callback should be called at the start of navigation and every
  // redirect, until |should_ignore_| is true.
  CheckCallback should_ignore_callback_;

  // Note that the CheckCallback currently has thread affinity on the Java side.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // How many outbound pending checks are running. Normally this will be either
  // 0 or 1, but making this a bool makes too many assumptions about the nature
  // of Chrome's task queues (e.g. we could be scheduled after the task which
  // redirects the navigation).
  int pending_checks_ = 0;

  // Whether the navigation should be ignored. Updated at every redirect.
  bool should_ignore_ = false;

  // Whether the navigation is currently deferred.
  bool deferring_ = false;

  base::WeakPtrFactory<InterceptNavigationThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterceptNavigationThrottle);
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_
