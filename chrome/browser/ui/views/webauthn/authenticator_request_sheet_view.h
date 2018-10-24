// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class AuthenticatorRequestSheetModel;

// Defines the basic structure of sheets shown in the authenticator request
// dialog. Each sheet corresponds to a given step of the authentication flow,
// and encapsulates the controls above the Ok/Cancel buttons, namely:
//  -- an optional `back icon`,
//  -- the title of the current step,
//  -- the description of the current step, and
//  -- an optional view with step-specific content, added by subclasses, filling
//     the rest of the space.
//
// +-------------------------------------------------+
// | (<-)  Title of the current step                 |
// |                                                 |
// | Description text explaining to the user what    |
// | this step is all about.                         |
// |                                                 |
// | +---------------------------------------------+ |
// | |                                             | |
// | |          Step-specific content view         | |
// | |                                             | |
// | |                                             | |
// | +---------------------------------------------+ |
// +-------------------------------------------------+
// |                                   OK   CANCEL   | <- Not part of this view.
// +-------------------------------------------------+
//
// TODO(https://crbug.com/852352): The Web Authentication and Web Payment APIs
// both use the concept of showing multiple "sheets" in a single dialog. To
// avoid code duplication, consider factoring out common parts.
class AuthenticatorRequestSheetView : public views::View,
                                      public views::ButtonListener {
 public:
  explicit AuthenticatorRequestSheetView(
      std::unique_ptr<AuthenticatorRequestSheetModel> model);
  ~AuthenticatorRequestSheetView() override;

  // Creates the standard child views on this sheet, potentially including
  // step-specific content if any.
  void InitChildViews();

  AuthenticatorRequestSheetModel* model() { return model_.get(); }

 protected:
  // Returns the step-specific view the derived sheet wishes to provide, if any.
  virtual std::unique_ptr<views::View> BuildStepSpecificContent();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // Creates the header row of the sheet, containing an optional back arrow,
  // followed by the title of the sheet.
  std::unique_ptr<views::View> CreateHeaderRow();

  std::unique_ptr<AuthenticatorRequestSheetModel> model_;
  views::Button* back_arrow_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestSheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_REQUEST_SHEET_VIEW_H_
