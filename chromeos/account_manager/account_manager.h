// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_H_
#define CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_H_

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chromeos/account_manager/tokens.pb.h"
#include "chromeos/chromeos_export.h"

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace base {
class SequencedTaskRunner;
class ImportantFileWriter;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace chromeos {

class CHROMEOS_EXPORT AccountManager {
 public:
  struct AccountKey {
    // |id| is obfuscated GAIA id for |AccountType::ACCOUNT_TYPE_GAIA|.
    // |id| is object GUID (|AccountId::GetObjGuid|) for
    // |AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY|.
    std::string id;
    account_manager::AccountType account_type;

    bool IsValid() const;

    bool operator<(const AccountKey& other) const;
    bool operator==(const AccountKey& other) const;
  };

  // A map from |AccountKey| to a raw token.
  using TokenMap = std::map<AccountKey, std::string>;

  // A callback for list of |AccountKey|s.
  using AccountListCallback = base::OnceCallback<void(std::vector<AccountKey>)>;

  using DelayNetworkCallRunner =
      base::RepeatingCallback<void(const base::RepeatingClosure&)>;

  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // Called when the token for |account_key| is updated/inserted.
    // Use |AccountManager::AddObserver| to add an |Observer|.
    // Note: |Observer|s which register with |AccountManager| before its
    // initialization is complete will get notified when |AccountManager| is
    // fully initialized.
    // Note: |Observer|s which register with |AccountManager| after its
    // initialization is complete will not get an immediate
    // notification-on-registration.
    virtual void OnTokenUpserted(const AccountKey& account_key) = 0;

    // Called when an account has been removed from AccountManager.
    // Observers that may have cached access tokens (fetched via
    // |AccountManager::CreateAccessTokenFetcher|), must clear their cache entry
    // for this |account_key| on receiving this callback.
    virtual void OnAccountRemoved(const AccountKey& account_key) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Note: |Initialize| MUST be called at least once on this object.
  AccountManager();
  virtual ~AccountManager();

  // |home_dir| is the path of the Device Account's home directory (root of the
  // user's cryptohome).
  // |request_context| is a non-owning pointer.
  // |delay_network_call_runner| is basically a wrapper for
  // |chromeos::DelayNetworkCall|. Cannot use |chromeos::DelayNetworkCall| due
  // to linking/dependency constraints.
  // This method MUST be called at least once in the lifetime of AccountManager.
  void Initialize(const base::FilePath& home_dir,
                  net::URLRequestContextGetter* request_context,
                  DelayNetworkCallRunner delay_network_call_runner);

  // Gets (async) a list of account keys known to |AccountManager|.
  void GetAccounts(AccountListCallback callback);

  // Removes an account. Does not do anything if |account_key| is not known by
  // |AccountManager|.
  // Observers are notified about an account removal through
  // |Observer::OnAccountRemoved|.
  // If the account being removed is a GAIA account, a token revocation with
  // GAIA is also attempted, on a best effort basis. Even if token revocation
  // with GAIA fails, AccountManager will forget the account.
  void RemoveAccount(const AccountKey& account_key);

  // Updates or inserts a token, for the account corresponding to the given
  // |account_key|. |account_key| must be valid (|AccountKey::IsValid|).
  void UpsertToken(const AccountKey& account_key, const std::string& token);

  // Add a non owning pointer to an |AccountManager::Observer|.
  void AddObserver(Observer* observer);

  // Removes an |AccountManager::Observer|. Does nothing if the |observer| is
  // not in the list of known observers.
  void RemoveObserver(Observer* observer);

  // Gets AccountManager's URL Request Context.
  net::URLRequestContextGetter* GetUrlRequestContext();

  // Creates and returns an |OAuth2AccessTokenFetcher| using the refresh token
  // stored for |account_key|. |IsTokenAvailable| should be |true| for
  // |account_key|, otherwise a |nullptr| is returned.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const AccountKey& account_key,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) const;

  // Returns |true| if an LST is available for |account_key|.
  // Note: An LST will not be available for |account_key| if it is an Active
  // Directory account.
  // Note: This method will return |false| if |AccountManager| has not been
  // initialized yet.
  bool IsTokenAvailable(const AccountKey& account_key) const;

 private:
  enum InitializationState {
    kNotStarted,   // Initialize has not been called
    kInProgress,   // Initialize has been called but not completed
    kInitialized,  // Initialization was successfully completed
  };

  // A util class to revoke Gaia tokens on server. This class is meant to be
  // used for a single request.
  class GaiaTokenRevocationRequest;

  friend class AccountManagerTest;
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest, TestInitialization);

  // Same as the public |Initialize| except for a |task_runner|.
  void Initialize(const base::FilePath& home_dir,
                  net::URLRequestContextGetter* request_context,
                  DelayNetworkCallRunner delay_network_call_runner,
                  scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Reads tokens from |tokens| and inserts them in |tokens_| and runs all
  // callbacks waiting on |AccountManager| initialization.
  void InsertTokensAndRunInitializationCallbacks(const TokenMap& tokens);

  // Accepts a closure and runs it immediately if |AccountManager| has already
  // been initialized, otherwise saves the |closure| for running later, when the
  // class is initialized.
  void RunOnInitialization(base::OnceClosure closure);

  // Does the actual work of getting a list of accounts. Assumes that
  // |AccountManager| initialization (|init_state_|) is complete.
  void GetAccountsInternal(AccountListCallback callback);

  // Does the actual work of removing an account. Assumes that
  // |AccountManager| initialization (|init_state_|) is complete.
  void RemoveAccountInternal(const AccountKey& account_key);

  // Does the actual work of updating or inserting tokens. Assumes that
  // |AccountManager| initialization (|init_state_|) is complete.
  void UpsertTokenInternal(const AccountKey& account_key,
                           const std::string& token);

  // Posts a task on |task_runner_|, which is usually a background thread, to
  // persist the current state of |tokens_|.
  void PersistTokensAsync();

  // Notify |Observer|s about a token update.
  void NotifyTokenObservers(const AccountKey& account_key);

  // Notify |Observer|s about an account removal.
  void NotifyAccountRemovalObservers(const AccountKey& account_key);

  // Revokes |account_key|'s token on the relevant backend.
  // Note: Does not do anything if the |account_manager::AccountType|
  // of |account_key| does not support server token revocation.
  // Note: Does not do anything if |account_key| is not present in |tokens_|.
  // Hence, call this method before actually modifying or deleting old tokens
  // from |tokens_|.
  void MaybeRevokeTokenOnServer(const AccountKey& account_key);

  // Revokes |refresh_token| with GAIA. Virtual for testing.
  virtual void RevokeGaiaTokenOnServer(const std::string& refresh_token);

  // Called by |GaiaTokenRevocationRequest| to notify its request completion.
  // Deletes |request| from |pending_token_revocation_requests_|, if present.
  void DeletePendingTokenRevocationRequest(GaiaTokenRevocationRequest* request);

  // Status of this object's initialization.
  InitializationState init_state_ = InitializationState::kNotStarted;

  // All tokens, if channel bound, are bound to |request_context_|. This is a
  // non-owning pointer.
  net::URLRequestContextGetter* request_context_ = nullptr;

  // An indirect way to access |chromeos::DelayNetworkCall|. We cannot use
  // |chromeos::DelayNetworkCall| directly here due to linking/dependency
  // issues.
  DelayNetworkCallRunner delay_network_call_runner_;

  // A task runner for disk I/O.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::ImportantFileWriter> writer_;

  // A map of account keys to tokens.
  TokenMap tokens_;

  // Callbacks waiting on class initialization (|init_state_|).
  std::vector<base::OnceClosure> initialization_callbacks_;

  // A list of |AccountManager| observers.
  // Verifies that the list is empty on destruction.
  base::ObserverList<Observer, true /* check_empty */> observers_;

  // A list of pending token revocation requests.
  // |AccountManager| is a long living object in general and these requests are
  // basically one shot fire-and-forget requests but for ASAN tests, we do not
  // want to have dangling pointers to pending requests.
  std::vector<std::unique_ptr<GaiaTokenRevocationRequest>>
      pending_token_revocation_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountManager> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AccountManager);
};

// For logging.
CHROMEOS_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const AccountManager::AccountKey& account_key);

}  // namespace chromeos

#endif  // CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_H_
