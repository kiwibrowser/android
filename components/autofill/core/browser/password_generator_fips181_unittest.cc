// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string.h>

#include <locale>

#include "components/autofill/core/browser/password_generator_fips181.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CheckPasswordCorrectness(const std::string& password) {
  int num_upper_case_letters = 0;
  int num_lower_case_letters = 0;
  int num_digits = 0;
  for (size_t i = 0; i < password.size(); i++) {
    if (isupper(password[i]))
      ++num_upper_case_letters;
    else if (islower(password[i]))
      ++num_lower_case_letters;
    else if (isdigit(password[i]))
      ++num_digits;
  }
  EXPECT_GT(num_upper_case_letters, 0) << password;
  EXPECT_GT(num_lower_case_letters, 0) << password;
  EXPECT_GT(num_digits, 0) << password;
}

const char* g_password_text = nullptr;

int GenerateForTest(char* word,
                    char* hypenated_word,
                    unsigned short minlen,
                    unsigned short maxlen,
                    unsigned int pass_mode) {
  EXPECT_LE(minlen, maxlen);
  EXPECT_TRUE(word);
  EXPECT_TRUE(hypenated_word);
  EXPECT_TRUE(g_password_text) << "Set g_password_text before every call";
  strncpy(word, g_password_text, maxlen);
  g_password_text = nullptr;
  // Resize password to |maxlen|.
  word[maxlen] = '\0';
  EXPECT_GE(strlen(word), minlen)
      << "Make sure to provide enough characters in g_password_text";
  return static_cast<int>(strlen(word));
}

class PasswordGeneratorFips181Test : public ::testing::Test {
 public:
  PasswordGeneratorFips181Test() {
    autofill::PasswordGeneratorFips181::SetGeneratorForTest(GenerateForTest);
  }
  ~PasswordGeneratorFips181Test() override {
    autofill::PasswordGeneratorFips181::SetGeneratorForTest(nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordGeneratorFips181Test);
};

}  // namespace

namespace autofill {

TEST_F(PasswordGeneratorFips181Test, PasswordLength) {
  PasswordGeneratorFips181 pg1(10);
  g_password_text = "Aa12345678901234567890";
  std::string password = pg1.Generate();
  EXPECT_EQ(password.size(), 10u);

  PasswordGeneratorFips181 pg2(-1);
  g_password_text = "Aa12345678901234567890";
  password = pg2.Generate();
  EXPECT_EQ(
      password.size(),
      static_cast<size_t>(PasswordGeneratorFips181::kDefaultPasswordLength));

  PasswordGeneratorFips181 pg3(100);
  g_password_text = "Aa12345678901234567890";
  password = pg3.Generate();
  EXPECT_EQ(
      password.size(),
      static_cast<size_t>(PasswordGeneratorFips181::kDefaultPasswordLength));
}

TEST_F(PasswordGeneratorFips181Test, PasswordPattern) {
  PasswordGeneratorFips181 pg1(12);
  g_password_text = "012345678jkl";
  std::string password1 = pg1.Generate();
  CheckPasswordCorrectness(password1);

  PasswordGeneratorFips181 pg2(12);
  g_password_text = "abcDEFGHIJKL";
  std::string password2 = pg2.Generate();
  CheckPasswordCorrectness(password2);

  PasswordGeneratorFips181 pg3(12);
  g_password_text = "abcdefghijkl";
  std::string password3 = pg3.Generate();
  CheckPasswordCorrectness(password3);
}

TEST_F(PasswordGeneratorFips181Test, ForceFixPasswordTest) {
  std::string passwords_to_fix[] = {"nonumbersoruppercase",
                                    "nonumbersWithuppercase",
                                    "numbers3Anduppercase", "UmpAwgemHoc"};
  for (auto& password : passwords_to_fix) {
    ForceFixPassword(&password);
    CheckPasswordCorrectness(password);
  }
}

}  // namespace autofill
