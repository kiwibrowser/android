// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_

#include <vector>

namespace autofill {
struct PasswordForm;
}

namespace content {
class RenderFrameHost;
}

namespace password_manager {
// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// See also content/browser/bad_message.h.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms.
enum class BadMessageReason {
  CPMD_BAD_ORIGIN_FORMS_PARSED = 1,
  CPMD_BAD_ORIGIN_FORMS_RENDERED = 2,
  CPMD_BAD_ORIGIN_FORM_SUBMITTED = 3,
  CPMD_BAD_ORIGIN_FOCUSED_PASSWORD_FORM_FOUND = 4,
  CPMD_BAD_ORIGIN_IN_PAGE_NAVIGATION = 5,
  CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED = 6,
  CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD = 7,
  CPMD_BAD_ORIGIN_SAVE_GENERATION_FIELD_DETECTED_BY_CLASSIFIER = 8,
  CPMD_BAD_ORIGIN_SHOW_FALLBACK_FOR_SAVING = 9,

  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. ContentPasswordManagerDriver becomes CPMD) plus a unique
  // description of the reason. After making changes, you MUST update
  // histograms.xml by running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  BAD_MESSAGE_MAX
};

namespace bad_message {
// Returns true if the renderer for |frame| is allowed to perform an operation
// on |password_form|. If the origin mismatches, the process for |frame| is
// terminated and the function returns false.
bool CheckChildProcessSecurityPolicy(
    content::RenderFrameHost* frame,
    const autofill::PasswordForm& password_form,
    BadMessageReason reason);

// Same as above but checks every form in |forms|.
bool CheckChildProcessSecurityPolicy(
    content::RenderFrameHost* frame,
    const std::vector<autofill::PasswordForm>& forms,
    BadMessageReason reason);

}  // namespace bad_message
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_
