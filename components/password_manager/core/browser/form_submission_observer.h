// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SUBMISSION_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SUBMISSION_OBSERVER_H_

namespace password_manager {

class PasswordManagerDriver;

// Observer interface for the password manager about the relevant events from
// the embedder.
class FormSubmissionObserver {
 public:
  // Notifies the listener that a form may be submitted due to a navigation.
  virtual void OnStartNavigation(PasswordManagerDriver* driver) = 0;

 protected:
  virtual ~FormSubmissionObserver() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SUBMISSION_OBSERVER_H_
