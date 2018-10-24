// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_CONTROLLER_H_
#define CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "net/url_request/url_request_context_getter.h"

class PrefService;

namespace policy {
class MachineLevelUserCloudPolicyManager;
class MachineLevelUserCloudPolicyFetcher;
class MachineLevelUserCloudPolicyRegisterWatcher;
class MachineLevelUserCloudPolicyRegistrar;

// A class that setups and manages MachineLevelUserCloudPolicy.
class MachineLevelUserCloudPolicyController {
 public:
  // Machine level user cloud policy enrollment result.
  enum class RegisterResult {
    kNoEnrollmentNeeded,  // The device won't be enrolled without an enrollment
                          // token.
    kEnrollmentSuccess,   // The device has been enrolled successfully.
    kQuitDueToFailure,  // The enrollment has failed or aborted, user choose to
                        // quit Chrome.
    kRestartDueToFailure,  // The enrollment has failed, user choose to restart
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when policy enrollment is finished.
    // |succeeded| is true if |dm_token| is returned from the server.
    virtual void OnPolicyRegisterFinished(bool succeeded) {}
  };

  // Directory name under the user-data-dir where the policy data is stored.
  static const base::FilePath::CharType kPolicyDir[];

  MachineLevelUserCloudPolicyController();
  virtual ~MachineLevelUserCloudPolicyController();

  static std::unique_ptr<MachineLevelUserCloudPolicyManager>
  CreatePolicyManager();

  void Init(PrefService* local_state,
            scoped_refptr<net::URLRequestContextGetter> request_context);

  RegisterResult WaitUntilPolicyEnrollmentFinished();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyPolicyRegisterFinished(bool succeeded);

 private:
  bool GetEnrollmentTokenAndClientId(std::string* enrollment_token,
                                     std::string* client_id);
  void RegisterForPolicyWithEnrollmentTokenCallback(
      const std::string& dm_token,
      const std::string& client_id);

  base::ObserverList<Observer, true> observers_;

  std::unique_ptr<MachineLevelUserCloudPolicyRegistrar> policy_registrar_;
  std::unique_ptr<MachineLevelUserCloudPolicyFetcher> policy_fetcher_;
    // This is an observer of the controller and needs to be declared after the
    // |observers_|.
  std::unique_ptr<MachineLevelUserCloudPolicyRegisterWatcher>
      policy_register_watcher_;

  // Time at which the enrollment process was started.  Used to log UMA metric.
  base::Time enrollment_start_time_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyController);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MACHINE_LEVEL_USER_CLOUD_POLICY_CONTROLLER_H_
