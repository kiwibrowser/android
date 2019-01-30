// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_

#include <string>

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

class AutofillManagerTestDelegateImpl
    : public autofill::AutofillManagerTestDelegate {
 public:
  AutofillManagerTestDelegateImpl();
  ~AutofillManagerTestDelegateImpl() override;

  // autofill::AutofillManagerTestDelegate:
  void DidPreviewFormData() override;
  void DidFillFormData() override;
  void DidShowSuggestions() override;
  void OnTextFieldChanged() override;

  void Reset();
  void Wait();
  void WaitForTextChange();
  bool WaitForPreviewFormData(base::TimeDelta timeout);
  bool WaitForFormDataFilled(base::TimeDelta timeout);
  bool WaitForSuggestionShown(base::TimeDelta timeout);
  bool WaitForTextChange(base::TimeDelta timeout);
  void SetIsExpectingDynamicRefill(bool expect_refill) {
    is_expecting_dynamic_refill_ = expect_refill;
  }

 private:
  base::RunLoop* run_loop_;
  bool is_expecting_dynamic_refill_;
  bool waiting_for_preview_form_data_;
  bool waiting_for_fill_form_data_;
  bool waiting_for_show_suggestion_;
  bool waiting_for_text_change_;

  DISALLOW_COPY_AND_ASSIGN(AutofillManagerTestDelegateImpl);
};

class AutofillUiTest : public InProcessBrowserTest {
 protected:
  AutofillUiTest();
  ~AutofillUiTest() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  bool TryFillForm(const std::string& focus_element_xpath,
                   const int attempts = 1);
  bool ShowAutofillSuggestion(const std::string& focus_element_xpath);

  void SendKeyToPageAndWait(ui::DomKey key);
  void SendKeyToPageAndWait(ui::DomKey key,
                            ui::DomCode code,
                            ui::KeyboardCode key_code);
  void SendKeyToPopupAndWait(ui::DomKey key);
  void SendKeyToPopupAndWait(ui::DomKey key,
                             ui::DomCode code,
                             ui::KeyboardCode key_code,
                             content::RenderWidgetHost* widget);
  void SendKeyToDataListPopup(ui::DomKey key);
  void SendKeyToDataListPopup(ui::DomKey key,
                              ui::DomCode code,
                              ui::KeyboardCode key_code);
  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  content::WebContents* GetWebContents();
  content::RenderViewHost* GetRenderViewHost();
  AutofillManager* GetAutofillManager();

  AutofillManagerTestDelegateImpl* test_delegate() { return &test_delegate_; }
  content::RenderWidgetHost::KeyPressEventCallback key_press_event_sink();

 private:
  void SendKeyToPage(ui::DomKey key);
  void SendKeyToPopup(ui::DomKey key);

  AutofillManagerTestDelegateImpl test_delegate_;

  // KeyPressEventCallback that serves as a sink to ensure that every key press
  // event the tests create and have the WebContents forward is handled by some
  // key press event callback. It is necessary to have this sink because if no
  // key press event callback handles the event (at least on Mac), a DCHECK
  // ends up going off that the |event| doesn't have an |os_event| associated
  // with it.
  content::RenderWidgetHost::KeyPressEventCallback key_press_event_sink_;

  DISALLOW_COPY_AND_ASSIGN(AutofillUiTest);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_
