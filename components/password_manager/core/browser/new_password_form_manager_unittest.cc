// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormStructure;
using autofill::FormFieldData;
using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using base::TestMockTimeTaskRunner;
using testing::_;
using testing::Mock;
using testing::SaveArg;

namespace password_manager {

namespace {
class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {}

  ~MockPasswordManagerDriver() override {}

  MOCK_METHOD1(FillPasswordForm, void(const PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(const PasswordForm&));
};

}  // namespace

class NewPasswordFormManagerTest : public testing::Test {
 public:
  NewPasswordFormManagerTest() : task_runner_(new TestMockTimeTaskRunner) {
    GURL origin = GURL("http://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("http://accounts.google.com/a/ServiceLogin");

    observed_form_.origin = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = 1;
    observed_form_.is_form_tag = true;

    FormFieldData field;
    field.name = ASCIIToUTF16("firstname");
    field.form_control_type = "text";
    field.unique_renderer_id = 1;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("username");
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("password");
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    observed_form_.fields.push_back(field);

    saved_match_.origin = origin;
    saved_match_.action = action;
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");
  }

 protected:
  FormData observed_form_;
  PasswordForm saved_match_;
  StubPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
};

TEST_F(NewPasswordFormManagerTest, DoesManage) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  EXPECT_TRUE(form_manager.DoesManage(observed_form_, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager.DoesManage(observed_form_, nullptr));
  FormData another_form = observed_form_;
  another_form.is_form_tag = false;
  EXPECT_FALSE(form_manager.DoesManage(another_form, &driver_));

  another_form = observed_form_;
  another_form.unique_renderer_id = observed_form_.unique_renderer_id + 1;
  EXPECT_FALSE(form_manager.DoesManage(another_form, &driver_));
}

TEST_F(NewPasswordFormManagerTest, DoesManageNoFormTag) {
  observed_form_.is_form_tag = false;
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  FormData another_form = observed_form_;
  // Simulate that new input was added by JavaScript.
  another_form.fields.push_back(FormFieldData());
  EXPECT_TRUE(form_manager.DoesManage(another_form, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager.DoesManage(another_form, nullptr));
}

TEST_F(NewPasswordFormManagerTest, Autofill) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  EXPECT_CALL(driver_, AllowPasswordGenerationForForm(_));
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.origin, fill_data.origin);
  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(observed_form_.fields[1].name, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.fields[2].name, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(NewPasswordFormManagerTest, NoAutofillSignUpForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  // Make |observed_form_| to be sign-up form.
  observed_form_.fields.back().autocomplete_attribute = "new-password";

  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(NewPasswordFormManagerTest, SetSubmitted) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  EXPECT_FALSE(form_manager.is_submitted());
  EXPECT_TRUE(
      form_manager.SetSubmittedFormIfIsManaged(observed_form_, &driver_));
  EXPECT_TRUE(form_manager.is_submitted());

  FormData another_form = observed_form_;
  another_form.name += ASCIIToUTF16("1");
  // |another_form| is managed because the same |unique_renderer_id| as
  // |observed_form_|.
  EXPECT_TRUE(form_manager.SetSubmittedFormIfIsManaged(another_form, &driver_));
  EXPECT_TRUE(form_manager.is_submitted());

  form_manager.set_not_submitted();
  EXPECT_FALSE(form_manager.is_submitted());

  another_form.unique_renderer_id = observed_form_.unique_renderer_id + 1;
  EXPECT_FALSE(
      form_manager.SetSubmittedFormIfIsManaged(another_form, &driver_));
  EXPECT_FALSE(form_manager.is_submitted());

  // An identical form but in a different frame (represented here by a null
  // driver) is also not considered managed.
  EXPECT_FALSE(
      form_manager.SetSubmittedFormIfIsManaged(observed_form_, nullptr));
  EXPECT_FALSE(form_manager.is_submitted());
}

// Tests that when NewPasswordFormManager receives saved matches it waits for
// server predictions and fills on receving them.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsWithinDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();

  // Expects no filling on save matches receiving.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);
  Mock::VerifyAndClearExpectations(&driver_);

  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  form_manager.ProcessServerPredictions(predictions);
}

// Tests that NewPasswordFormManager fills after some delay even without
// server predictions.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsAfterDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FakeFormFetcher fetcher;
  fetcher.Fetch();

  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  fetcher.SetNonFederated({&saved_match_}, 0u);
  // Expect filling after passing filling delay.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  // Simulate passing filling delay.
  task_runner_->FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};

  // Expect no filling on receiving server predictions because it was already
  // done.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager.ProcessServerPredictions(predictions);
  task_runner_->FastForwardUntilNoTasksRemain();
}

// Tests that filling happens immediately if server predictions are received
// before saved matches.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsBeforeFetcher) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  // Expect no filling after receiving saved matches from |fetcher|, since
  // |form_manager| is waiting for server-side predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  NewPasswordFormManager form_manager(&client_, driver_.AsWeakPtr(),
                                      observed_form_, &fetcher);
  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};
  form_manager.ProcessServerPredictions(predictions);
  Mock::VerifyAndClearExpectations(&driver_);

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  fetcher.SetNonFederated({&saved_match_}, 0u);
}

}  // namespace  password_manager
