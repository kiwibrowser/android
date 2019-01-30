// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/events/base_event_utils.h"

namespace autofill {

// AutofillManagerTestDelegateImpl --------------------------------------------
AutofillManagerTestDelegateImpl::AutofillManagerTestDelegateImpl()
    : run_loop_(nullptr),
      is_expecting_dynamic_refill_(false),
      waiting_for_preview_form_data_(false),
      waiting_for_fill_form_data_(false),
      waiting_for_show_suggestion_(false),
      waiting_for_text_change_(false) {}

AutofillManagerTestDelegateImpl::~AutofillManagerTestDelegateImpl() {}

void AutofillManagerTestDelegateImpl::DidPreviewFormData() {
  if (!waiting_for_preview_form_data_)
    return;
  waiting_for_preview_form_data_ = false;
  run_loop_->Quit();
}

void AutofillManagerTestDelegateImpl::DidFillFormData() {
  if (!is_expecting_dynamic_refill_)
    ASSERT_TRUE(run_loop_->running());
  if (!waiting_for_fill_form_data_)
    return;
  waiting_for_fill_form_data_ = false;
  run_loop_->Quit();
}

void AutofillManagerTestDelegateImpl::DidShowSuggestions() {
  if (!waiting_for_show_suggestion_)
    return;
  waiting_for_show_suggestion_ = false;
  run_loop_->Quit();
}

void AutofillManagerTestDelegateImpl::OnTextFieldChanged() {
  if (!waiting_for_text_change_)
    return;
  waiting_for_text_change_ = false;
  run_loop_->Quit();
}

void AutofillManagerTestDelegateImpl::Reset() {
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, nullptr);
  waiting_for_preview_form_data_ = false;
  waiting_for_fill_form_data_ = false;
  waiting_for_show_suggestion_ = false;
  waiting_for_text_change_ = false;
}

void AutofillManagerTestDelegateImpl::Wait() {
  waiting_for_preview_form_data_ = true;
  waiting_for_fill_form_data_ = true;
  waiting_for_show_suggestion_ = true;
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  run_loop_->Run();
}

void AutofillManagerTestDelegateImpl::WaitForTextChange() {
  waiting_for_text_change_ = true;
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  run_loop_->Run();
}

bool AutofillManagerTestDelegateImpl::WaitForPreviewFormData(
    base::TimeDelta timeout = base::TimeDelta::FromSeconds(0)) {
  waiting_for_preview_form_data_ = true;
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  if (!timeout.is_zero()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), timeout);
  }
  run_loop_->Run();
  return !waiting_for_preview_form_data_;
}

bool AutofillManagerTestDelegateImpl::WaitForFormDataFilled(
    base::TimeDelta timeout = base::TimeDelta::FromSeconds(0)) {
  waiting_for_fill_form_data_ = true;
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  if (!timeout.is_zero()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), timeout);
  }
  run_loop_->Run();
  return !waiting_for_fill_form_data_;
}

bool AutofillManagerTestDelegateImpl::WaitForSuggestionShown(
    base::TimeDelta timeout = base::TimeDelta::FromSeconds(0)) {
  waiting_for_show_suggestion_ = true;
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  if (!timeout.is_zero()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), timeout);
  }
  run_loop_->Run();
  return !waiting_for_show_suggestion_;
}

bool AutofillManagerTestDelegateImpl::WaitForTextChange(
    base::TimeDelta timeout = base::TimeDelta::FromSeconds(0)) {
  waiting_for_text_change_ = true;
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  if (!timeout.is_zero()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), timeout);
  }
  run_loop_->Run();
  return !waiting_for_text_change_;
}

// AutofillUiTest ----------------------------------------------------
AutofillUiTest::AutofillUiTest()
    : key_press_event_sink_(
          base::BindRepeating(&AutofillUiTest::HandleKeyPressEvent,
                              base::Unretained(this))) {}

AutofillUiTest::~AutofillUiTest() {}

void AutofillUiTest::SetUpOnMainThread() {
  // Don't want Keychain coming up on Mac.
  test::DisableSystemServices(browser()->profile()->GetPrefs());

  // Inject the test delegate into the AutofillManager.
  GetAutofillManager()->SetTestDelegate(test_delegate());

  // If the mouse happened to be over where the suggestions are shown, then
  // the preview will show up and will fail the tests. We need to give it a
  // point that's within the browser frame, or else the method hangs.
  gfx::Point reset_mouse(GetWebContents()->GetContainerBounds().origin());
  reset_mouse = gfx::Point(reset_mouse.x() + 5, reset_mouse.y() + 5);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(reset_mouse));
}

void AutofillUiTest::TearDownOnMainThread() {
  // Make sure to close any showing popups prior to tearing down the UI.
  GetAutofillManager()->client()->HideAutofillPopup();
  test::ReenableSystemServices();
}

bool AutofillUiTest::TryFillForm(const std::string& focus_element_xpath,
                                 const int attempts) {
  content::WebContents* web_contents = GetWebContents();
  AutofillManager* autofill_manager =
      ContentAutofillDriverFactory::FromWebContents(web_contents)
          ->DriverForFrame(web_contents->GetMainFrame())
          ->autofill_manager();

  int tries = 0;
  while (tries < attempts) {
    tries++;
    autofill_manager->client()->HideAutofillPopup();

    if (!ShowAutofillSuggestion(focus_element_xpath)) {
      LOG(WARNING) << "Failed to bring up the autofill suggestion drop down.";
      continue;
    }

    // Press the down key again to highlight the first choice in the autofill
    // suggestion drop down.
    test_delegate()->Reset();
    SendKeyToPopup(ui::DomKey::ARROW_DOWN);
    if (!test_delegate()->WaitForPreviewFormData(
            base::TimeDelta::FromSeconds(5))) {
      LOG(WARNING) << "Failed to select an option from the"
                   << " autofill suggestion drop down.";
      continue;
    }

    // Press the enter key to invoke autofill using the first suggestion.
    test_delegate()->Reset();
    SendKeyToPopup(ui::DomKey::ENTER);
    if (!test_delegate()->WaitForFormDataFilled(
            base::TimeDelta::FromSeconds(5))) {
      LOG(WARNING) << "Failed to fill the form.";
      continue;
    }

    return true;
  }

  autofill_manager->client()->HideAutofillPopup();
  return false;
}

bool AutofillUiTest::ShowAutofillSuggestion(
    const std::string& focus_element_xpath) {
  const std::string js(base::StringPrintf(
      "try {"
      "  var element = automation_helper.getElementByXpath(`%s`);"
      "  while (document.activeElement !== element) {"
      "    element.focus();"
      "  }"
      "} catch(ex) {}",
      focus_element_xpath.c_str()));
  if (content::ExecuteScript(GetWebContents(), js)) {
    test_delegate()->Reset();
    SendKeyToPage(ui::DomKey::ARROW_DOWN);
    return test_delegate()->WaitForSuggestionShown(
        base::TimeDelta::FromSeconds(5));
  }
  return false;
}

void AutofillUiTest::SendKeyToPage(ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  content::SimulateKeyPress(GetWebContents(), key, code, key_code, false, false,
                            false, false);
}

void AutofillUiTest::SendKeyToPageAndWait(ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SendKeyToPageAndWait(key, code, key_code);
}

void AutofillUiTest::SendKeyToPageAndWait(ui::DomKey key,
                                          ui::DomCode code,
                                          ui::KeyboardCode key_code) {
  test_delegate()->Reset();
  content::SimulateKeyPress(GetWebContents(), key, code, key_code, false, false,
                            false, false);
  test_delegate()->Wait();
}

void AutofillUiTest::SendKeyToPopup(ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  content::RenderWidgetHost* widget = GetRenderViewHost()->GetWidget();

  // Route popup-targeted key presses via the render view host.
  content::NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                        blink::WebInputEvent::kNoModifiers,
                                        ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  widget->AddKeyPressEventCallback(key_press_event_sink_);
  widget->ForwardKeyboardEvent(event);
  widget->RemoveKeyPressEventCallback(key_press_event_sink_);
}

void AutofillUiTest::SendKeyToPopupAndWait(ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SendKeyToPopupAndWait(key, code, key_code, GetRenderViewHost()->GetWidget());
}

void AutofillUiTest::SendKeyToPopupAndWait(ui::DomKey key,
                                           ui::DomCode code,
                                           ui::KeyboardCode key_code,
                                           content::RenderWidgetHost* widget) {
  // Route popup-targeted key presses via the render view host.
  content::NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                        blink::WebInputEvent::kNoModifiers,
                                        ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  test_delegate()->Reset();
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  widget->AddKeyPressEventCallback(key_press_event_sink_);
  widget->ForwardKeyboardEvent(event);
  test_delegate()->Wait();
  widget->RemoveKeyPressEventCallback(key_press_event_sink_);
}

void AutofillUiTest::SendKeyToDataListPopup(ui::DomKey key) {
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SendKeyToDataListPopup(key, code, key_code);
}

// Datalist does not support autofill preview. There is no need to start
// message loop for Datalist.
void AutofillUiTest::SendKeyToDataListPopup(ui::DomKey key,
                                            ui::DomCode code,
                                            ui::KeyboardCode key_code) {
  // Route popup-targeted key presses via the render view host.
  content::NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                        blink::WebInputEvent::kNoModifiers,
                                        ui::EventTimeForNow());
  event.windows_key_code = key_code;
  event.dom_code = static_cast<int>(code);
  event.dom_key = key;
  // Install the key press event sink to ensure that any events that are not
  // handled by the installed callbacks do not end up crashing the test.
  GetRenderViewHost()->GetWidget()->AddKeyPressEventCallback(
      key_press_event_sink_);
  GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
  GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_event_sink_);
}

bool AutofillUiTest::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  return true;
}

content::WebContents* AutofillUiTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::RenderViewHost* AutofillUiTest::GetRenderViewHost() {
  return GetWebContents()->GetRenderViewHost();
}

AutofillManager* AutofillUiTest::GetAutofillManager() {
  content::WebContents* web_contents = GetWebContents();
  return ContentAutofillDriverFactory::FromWebContents(web_contents)
      ->DriverForFrame(web_contents->GetMainFrame())
      ->autofill_manager();
}

}  // namespace autofill
