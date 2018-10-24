// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller.h"

#include <vector>

#include "chrome/browser/password_manager/password_accessory_view_interface.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::PasswordForm;
using Item = PasswordAccessoryViewInterface::AccessoryItem;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PasswordAccessoryController);

PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents)
    : container_view_(web_contents->GetNativeView()),
      view_(PasswordAccessoryViewInterface::Create(this)) {}

// Additional creation functions in unit tests only:
PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view)
    : container_view_(web_contents->GetNativeView()), view_(std::move(view)) {}

PasswordAccessoryController::~PasswordAccessoryController() = default;

// static
void PasswordAccessoryController::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new PasswordAccessoryController(
                                web_contents, std::move(view))));
}

void PasswordAccessoryController::OnPasswordsAvailable(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const GURL& origin) {
  DCHECK(view_);
  std::vector<Item> items;
  base::string16 passwords_title_str = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE);
  items.emplace_back(passwords_title_str, passwords_title_str,
                     /*is_password=*/false, Item::Type::LABEL);
  if (best_matches.empty()) {
    base::string16 passwords_empty_str = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE);
    items.emplace_back(passwords_empty_str, passwords_empty_str,
                       /*is_password=*/false, Item::Type::LABEL);
  }
  for (const auto& pair : best_matches) {
    const PasswordForm* form = pair.second;
    base::string16 username = GetDisplayUsername(*form);
    items.emplace_back(username, username,
                       /*is_password=*/false, Item::Type::SUGGESTION);
    items.emplace_back(
        form->password_value,
        l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, username),
        /*is_password=*/true, Item::Type::SUGGESTION);
  }
  view_->OnItemsAvailable(origin, items);
}

void PasswordAccessoryController::OnFillingTriggered(
    const base::string16& textToFill) const {
  // TODO(fhorschig): Actually fill |textToFill| into focused field.
}

gfx::NativeView PasswordAccessoryController::container_view() const {
  return container_view_;
}
