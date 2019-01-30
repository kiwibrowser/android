// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_accessory_view_interface.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/password_form.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::UTF16ToASCII;
using base::UTF16ToWide;
using testing::ElementsAre;
using testing::Eq;
using testing::Mock;
using testing::NotNull;
using testing::PrintToString;
using testing::StrictMock;
using AccessoryItem = PasswordAccessoryViewInterface::AccessoryItem;
using ItemType = AccessoryItem::Type;

// The mock view mocks the platform-specific implementation. That also means
// that we have to care about the lifespan of the Controller because that would
// usually be responsibility of the view.
class MockPasswordAccessoryView : public PasswordAccessoryViewInterface {
 public:
  MockPasswordAccessoryView() = default;

  MOCK_METHOD2(OnItemsAvailable,
               void(const GURL& origin,
                    const std::vector<AccessoryItem>& items));
  MOCK_METHOD1(OnFillingTriggered, void(const base::string16& textToFill));
  MOCK_METHOD0(OnViewDestroyed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordAccessoryView);
};

// Pretty prints input for the |MatchesItem| matcher.
std::string PrintItem(const base::string16& text,
                      const base::string16& description,
                      bool is_password,
                      ItemType type) {
  return "has text \"" + base::UTF16ToASCII(text) + "\" and description \"" +
         base::UTF16ToASCII(description) + "\" and is " +
         (is_password ? "" : "not ") +
         "a password "
         "and type is " +
         PrintToString(static_cast<int>(type));
}

// Compares whether a given AccessoryItem had the given properties.
MATCHER_P4(MatchesItem,
           text,
           description,
           is_password,
           itemType,
           PrintItem(text, description, is_password, itemType)) {
  return arg.text == text && arg.is_password == is_password &&
         arg.content_description == description && arg.itemType == itemType;
}

// Creates a new map entry in the |first| element of the returned pair. The
// |second| element holds the PasswordForm that the |first| element points to.
// That way, the pointer only points to a valid address in the called scope.
std::pair<std::pair<base::string16, const PasswordForm*>,
          std::unique_ptr<const PasswordForm>>
CreateEntry(const std::string& username, const std::string& password) {
  PasswordForm form;
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  std::unique_ptr<const PasswordForm> form_ptr(
      new PasswordForm(std::move(form)));
  auto username_form_pair =
      std::make_pair(ASCIIToUTF16(username), form_ptr.get());
  return {std::move(username_form_pair), std::move(form_ptr)};
}

base::string16 password_for_str(const base::string16& user) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, user);
}

base::string16 password_for_str(const std::string& user) {
  return password_for_str(ASCIIToUTF16(user));
}

base::string16 passwords_empty_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE);
}

base::string16 passwords_title_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE);
}

base::string16 no_user_str() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
}

}  // namespace

// Automagically used to pretty-print AccessoryItems. Must be in same namespace.
void PrintTo(const AccessoryItem& item, std::ostream* os) {
  *os << "has text \"" << UTF16ToWide(item.text) << "\" and description \""
      << UTF16ToWide(item.content_description) << "\" and is "
      << (item.is_password ? "" : "not ") << "a password and type is "
      << PrintToString(static_cast<int>(item.itemType));
}

// Automagically used to pretty-print item vectors. Must be in same namespace.
void PrintTo(const std::vector<AccessoryItem>& items, std::ostream* os) {
  *os << "has " << items.size() << " elements where\n";
  for (size_t index = 0; index < items.size(); ++index) {
    *os << "element #" << index << " ";
    PrintTo(items[index], os);
    *os << "\n";
  }
}

class PasswordAccessoryControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PasswordAccessoryController::CreateForWebContentsForTesting(
        web_contents(),
        std::make_unique<StrictMock<MockPasswordAccessoryView>>());
  }

  PasswordAccessoryController* controller() {
    return PasswordAccessoryController::FromWebContents(web_contents());
  }

  MockPasswordAccessoryView* view() {
    return static_cast<MockPasswordAccessoryView*>(controller()->view());
  }
};

TEST_F(PasswordAccessoryControllerTest, IsNotRecreatedForSameWebContents) {
  PasswordAccessoryController* initial_controller =
      PasswordAccessoryController::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordAccessoryController::CreateForWebContents(web_contents());
  EXPECT_EQ(PasswordAccessoryController::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordAccessoryControllerTest, TransformsMatchesToSuggestions) {
  EXPECT_CALL(
      *view(),
      OnItemsAvailable(
          GURL("https://example.com"),
          ElementsAre(
              MatchesItem(ASCIIToUTF16("Passwords"), ASCIIToUTF16("Passwords"),
                          false, ItemType::LABEL),
              MatchesItem(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                          ItemType::SUGGESTION),
              MatchesItem(ASCIIToUTF16("S3cur3"), password_for_str("Ben"), true,
                          ItemType::SUGGESTION))));

  controller()->OnPasswordsAvailable({CreateEntry("Ben", "S3cur3").first},
                                     GURL("https://example.com"));
}

TEST_F(PasswordAccessoryControllerTest, HintsToEmptyUserNames) {
  EXPECT_CALL(
      *view(),
      OnItemsAvailable(GURL("https://example.com"),
                       ElementsAre(MatchesItem(ASCIIToUTF16("Passwords"),
                                               ASCIIToUTF16("Passwords"), false,
                                               ItemType::LABEL),
                                   MatchesItem(no_user_str(), no_user_str(),
                                               false, ItemType::SUGGESTION),
                                   MatchesItem(ASCIIToUTF16("S3cur3"),
                                               password_for_str(no_user_str()),
                                               true, ItemType::SUGGESTION))));

  controller()->OnPasswordsAvailable({CreateEntry("", "S3cur3").first},
                                     GURL("https://example.com"));
}

TEST_F(PasswordAccessoryControllerTest, SortsAlphabeticalDuringTransform) {
  EXPECT_CALL(
      *view(),
      OnItemsAvailable(
          GURL("https://example.com"),
          ElementsAre(
              MatchesItem(passwords_title_str(), passwords_title_str(), false,
                          ItemType::LABEL),

              MatchesItem(ASCIIToUTF16("Alf"), ASCIIToUTF16("Alf"), false,
                          ItemType::SUGGESTION),
              MatchesItem(ASCIIToUTF16("PWD"), password_for_str("Alf"), true,
                          ItemType::SUGGESTION),

              MatchesItem(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                          ItemType::SUGGESTION),
              MatchesItem(ASCIIToUTF16("S3cur3"), password_for_str("Ben"), true,
                          ItemType::SUGGESTION),

              MatchesItem(ASCIIToUTF16("Cat"), ASCIIToUTF16("Cat"), false,
                          ItemType::SUGGESTION),
              MatchesItem(ASCIIToUTF16("M1@u"), password_for_str("Cat"), true,
                          ItemType::SUGGESTION),

              MatchesItem(ASCIIToUTF16("Zebra"), ASCIIToUTF16("Zebra"), false,
                          ItemType::SUGGESTION),
              MatchesItem(ASCIIToUTF16("M3h"), password_for_str("Zebra"), true,
                          ItemType::SUGGESTION))));

  controller()->OnPasswordsAvailable(
      {CreateEntry("Ben", "S3cur3").first, CreateEntry("Zebra", "M3h").first,
       CreateEntry("Alf", "PWD").first, CreateEntry("Cat", "M1@u").first},
      GURL("https://example.com"));
}

TEST_F(PasswordAccessoryControllerTest, ProvidesEmptySuggestionsMessage) {
  EXPECT_CALL(*view(), OnItemsAvailable(
                           GURL("https://example.com"),
                           ElementsAre(MatchesItem(ASCIIToUTF16("Passwords"),
                                                   ASCIIToUTF16("Passwords"),
                                                   false, ItemType::LABEL),
                                       MatchesItem(passwords_empty_str(),
                                                   passwords_empty_str(), false,
                                                   ItemType::LABEL))));

  controller()->OnPasswordsAvailable({}, GURL("https://example.com"));
}
