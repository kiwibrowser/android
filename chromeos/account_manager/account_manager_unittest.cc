// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/account_manager/account_manager.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace chromeos {

class AccountManagerSpy : public AccountManager {
 public:
  AccountManagerSpy() = default;
  ~AccountManagerSpy() override = default;

  MOCK_METHOD1(RevokeGaiaTokenOnServer, void(const std::string& refresh_token));

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerSpy);
};

class AccountManagerTest : public testing::Test {
 public:
  AccountManagerTest() = default;
  ~AccountManagerTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    request_context_ = new net::TestURLRequestContextGetter(
        scoped_task_environment_.GetMainThreadTaskRunner());
    ResetAndInitializeAccountManager();
  }

  // Gets the list of accounts stored in |account_manager_|.
  std::vector<AccountManager::AccountKey> GetAccountsBlocking() {
    std::vector<AccountManager::AccountKey> accounts;

    base::RunLoop run_loop;
    account_manager_->GetAccounts(base::BindOnce(
        [](std::vector<AccountManager::AccountKey>* accounts,
           base::OnceClosure quit_closure,
           std::vector<AccountManager::AccountKey> stored_accounts) -> void {
          *accounts = std::move(stored_accounts);
          std::move(quit_closure).Run();
        },
        base::Unretained(&accounts), run_loop.QuitClosure()));
    run_loop.Run();

    return accounts;
  }

  // Helper method to reset and initialize |account_manager_| with default
  // parameters.
  void ResetAndInitializeAccountManager() {
    account_manager_ = std::make_unique<AccountManagerSpy>();
    account_manager_->Initialize(tmp_dir_.GetPath(), request_context_.get(),
                                 immediate_callback_runner_,
                                 base::SequencedTaskRunnerHandle::Get());
  }

  // Check base/test/scoped_task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir tmp_dir_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  std::unique_ptr<AccountManagerSpy> account_manager_;
  const AccountManager::AccountKey kAccountKey_{
      "111", account_manager::AccountType::ACCOUNT_TYPE_GAIA};
  AccountManager::DelayNetworkCallRunner immediate_callback_runner_ =
      base::BindRepeating(
          [](const base::RepeatingClosure& closure) -> void { closure.Run(); });

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerTest);
};

class AccountManagerObserver : public AccountManager::Observer {
 public:
  AccountManagerObserver() = default;
  ~AccountManagerObserver() override = default;

  void OnTokenUpserted(const AccountManager::AccountKey& account_key) override {
    is_token_upserted_callback_called_ = true;
    accounts_.insert(account_key);
  }

  void OnAccountRemoved(
      const AccountManager::AccountKey& account_key) override {
    is_account_removed_callback_called_ = true;
    accounts_.erase(account_key);
  }

  bool is_token_upserted_callback_called_ = false;
  bool is_account_removed_callback_called_ = false;
  std::set<AccountManager::AccountKey> accounts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerObserver);
};

TEST(AccountManagerKeyTest, TestValidity) {
  AccountManager::AccountKey key1{
      std::string(), account_manager::AccountType::ACCOUNT_TYPE_GAIA};
  EXPECT_FALSE(key1.IsValid());

  AccountManager::AccountKey key2{
      "abc", account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED};
  EXPECT_FALSE(key2.IsValid());

  AccountManager::AccountKey key3{
      "abc", account_manager::AccountType::ACCOUNT_TYPE_GAIA};
  EXPECT_TRUE(key3.IsValid());
}

TEST_F(AccountManagerTest, TestInitialization) {
  AccountManager account_manager;

  EXPECT_EQ(account_manager.init_state_,
            AccountManager::InitializationState::kNotStarted);
  account_manager.Initialize(tmp_dir_.GetPath(), request_context_.get(),
                             immediate_callback_runner_,
                             base::SequencedTaskRunnerHandle::Get());
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(account_manager.init_state_,
            AccountManager::InitializationState::kInitialized);
}

TEST_F(AccountManagerTest, TestUpsert) {
  account_manager_->UpsertToken(kAccountKey_, "123");

  std::vector<AccountManager::AccountKey> accounts = GetAccountsBlocking();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(kAccountKey_, accounts[0]);
}

TEST_F(AccountManagerTest, TestPersistence) {
  account_manager_->UpsertToken(kAccountKey_, "123");
  scoped_task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();
  std::vector<AccountManager::AccountKey> accounts = GetAccountsBlocking();

  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(kAccountKey_, accounts[0]);
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnTokenInsertion) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->AddObserver(observer.get());

  account_manager_->UpsertToken(kAccountKey_, "123");
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer->is_token_upserted_callback_called_);
  EXPECT_EQ(1UL, observer->accounts_.size());
  EXPECT_EQ(kAccountKey_, *observer->accounts_.begin());

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnTokenUpdate) {
  auto observer = std::make_unique<AccountManagerObserver>();
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->AddObserver(observer.get());
  account_manager_->UpsertToken(kAccountKey_, "123");
  scoped_task_environment_.RunUntilIdle();

  // Observers should be called when token is updated.
  observer->is_token_upserted_callback_called_ = false;
  account_manager_->UpsertToken(kAccountKey_, "456");
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer->is_token_upserted_callback_called_);
  EXPECT_EQ(1UL, observer->accounts_.size());
  EXPECT_EQ(kAccountKey_, *observer->accounts_.begin());

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, ObserversAreNotNotifiedIfTokenIsNotUpdated) {
  auto observer = std::make_unique<AccountManagerObserver>();
  const std::string token = "123";
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->AddObserver(observer.get());
  account_manager_->UpsertToken(kAccountKey_, token);
  scoped_task_environment_.RunUntilIdle();

  // Observers should not be called when token is not updated.
  observer->is_token_upserted_callback_called_ = false;
  account_manager_->UpsertToken(kAccountKey_, token);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer->is_token_upserted_callback_called_);

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, RemovedAccountsAreImmediatelyUnavailable) {
  account_manager_->UpsertToken(kAccountKey_, "123");

  account_manager_->RemoveAccount(kAccountKey_);
  std::vector<AccountManager::AccountKey> accounts = GetAccountsBlocking();

  EXPECT_TRUE(accounts.empty());
}

TEST_F(AccountManagerTest, AccountRemovalIsPersistedToDisk) {
  account_manager_->UpsertToken(kAccountKey_, "123");
  account_manager_->RemoveAccount(kAccountKey_);
  scoped_task_environment_.RunUntilIdle();

  ResetAndInitializeAccountManager();

  std::vector<AccountManager::AccountKey> accounts = GetAccountsBlocking();

  EXPECT_TRUE(accounts.empty());
}

TEST_F(AccountManagerTest, ObserversAreNotifiedOnAccountRemoval) {
  auto observer = std::make_unique<AccountManagerObserver>();
  account_manager_->AddObserver(observer.get());
  account_manager_->UpsertToken(kAccountKey_, "123");
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(observer->is_account_removed_callback_called_);
  account_manager_->RemoveAccount(kAccountKey_);
  EXPECT_TRUE(observer->is_account_removed_callback_called_);
  EXPECT_TRUE(observer->accounts_.empty());

  account_manager_->RemoveObserver(observer.get());
}

TEST_F(AccountManagerTest, TokenRevocationIsAttemptedForGaiaAccountRemovals) {
  const std::string token = "123";
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(token));

  account_manager_->UpsertToken(kAccountKey_, token);
  scoped_task_environment_.RunUntilIdle();

  account_manager_->RemoveAccount(kAccountKey_);
}

TEST_F(AccountManagerTest,
       TokenRevocationIsNotAttemptedForNonGaiaAccountRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(_)).Times(0);

  const AccountManager::AccountKey adAccountKey{
      "999", account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY};
  account_manager_->UpsertToken(adAccountKey, "123");
  scoped_task_environment_.RunUntilIdle();

  account_manager_->RemoveAccount(adAccountKey);
}

TEST_F(AccountManagerTest, TokenRevocationIsNotAttemptedForEmptyTokenRemovals) {
  ResetAndInitializeAccountManager();
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(_)).Times(0);

  account_manager_->UpsertToken(kAccountKey_, std::string());
  scoped_task_environment_.RunUntilIdle();

  account_manager_->RemoveAccount(kAccountKey_);
}

TEST_F(AccountManagerTest, OldTokenIsRevokedOnTokenUpdate) {
  const std::string old_token = "123";
  const std::string new_token = "456";
  ResetAndInitializeAccountManager();
  // Only 1 token should be revoked.
  EXPECT_CALL(*account_manager_.get(), RevokeGaiaTokenOnServer(old_token))
      .Times(1);
  account_manager_->UpsertToken(kAccountKey_, old_token);

  // Update the token.
  account_manager_->UpsertToken(kAccountKey_, new_token);
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace chromeos
