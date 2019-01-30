// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

class PasswordAccessoryViewInterface;

// The controller for the view located below the keyboard accessory.
// Upon creation, it creates (and owns) a corresponding PasswordAccessoryView.
// This view will be provided with data and will notify this controller about
// interactions (like requesting to fill a password suggestions).
//
// Create it for a WebContents instance by calling:
//     PasswordAccessoryController::CreateForWebContents(web_contents);
// After that, it's attached to the |web_contents| instance and can be retrieved
// by calling:
//     PasswordAccessoryController::FromWebContents(web_contents);
// Any further calls to |CreateForWebContents| will be a noop.
class PasswordAccessoryController
    : public content::WebContentsUserData<PasswordAccessoryController> {
 public:
  ~PasswordAccessoryController() override;

  // Notifies the view about credentials to be displayed.
  void OnPasswordsAvailable(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const GURL& origin);

  // Called by the UI code to request that |textToFill| is to be filled into the
  // currently focused field.
  void OnFillingTriggered(const base::string16& textToFill) const;

  // The web page view containing the focused field.
  gfx::NativeView container_view() const;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a fake/mock view.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      std::unique_ptr<PasswordAccessoryViewInterface> test_view);

#if defined(UNIT_TEST)
  // Returns the held view for testing.
  PasswordAccessoryViewInterface* view() const { return view_.get(); }
#endif  // defined(UNIT_TEST)

 private:
  // Required for construction via |CreateForWebContents|:
  explicit PasswordAccessoryController(content::WebContents* contents);
  friend class content::WebContentsUserData<PasswordAccessoryController>;

  // Constructor that allows to inject a mock or fake view.
  PasswordAccessoryController(
      content::WebContents* web_contents,
      std::unique_ptr<PasswordAccessoryViewInterface> view);

  // The web page view this accessory sheet and the focused field live in.
  const gfx::NativeView container_view_;

  // Hold the native instance of the view. Must be last declared and initialized
  // member so the view can be created in the constructor with a fully set up
  // controller instance.
  std::unique_ptr<PasswordAccessoryViewInterface> view_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryController);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_H_
