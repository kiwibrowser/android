// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/form_submission_tracker_util.h"

#include "components/password_manager/core/browser/form_submission_observer.h"
#include "content/public/browser/navigation_handle.h"

namespace password_manager {

void NotifyOnStartNavigation(content::NavigationHandle* navigation_handle,
                             PasswordManagerDriver* driver,
                             FormSubmissionObserver* observer) {
  DCHECK(navigation_handle);
  DCHECK(observer);

  // Password manager isn't interested in
  // - subframe navigations,
  // - user initiated navigations (e.g. click on the bookmark),
  // - hyperlink navigations.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->IsRendererInitiated() ||
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_LINK))
    return;

  observer->OnStartNavigation(driver);
}

}  // namespace password_manager
