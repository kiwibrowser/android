// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_SUBMISSION_TRACKER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_SUBMISSION_TRACKER_UTIL_H_

#include "base/macros.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace password_manager {

class FormSubmissionObserver;
class PasswordManagerDriver;

// Calls |observer_| if the navigation is interesting for the password
// manager. |driver| is just passed to the observer. None content
// initiated, none top level, hyperlinks navigations are ignored.
void NotifyOnStartNavigation(content::NavigationHandle* navigation_handle,
                             PasswordManagerDriver* driver,
                             FormSubmissionObserver* observer);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_SUBMISSION_TRACKER_UTIL_H_
