// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace chromeos {

// Controlls enrollment flow for setting up Demo Mode.
class DemoSetupController
    : public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public policy::CloudPolicyStore::Observer {
 public:
  // Delegate that will be notified about result of setup flow when it is
  // finished.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the setup flow finished with error. |fatal| is true if the
    // error isn't recoverable and needs powerwash.
    virtual void OnSetupError(bool fatal) = 0;

    // Called when the setup flow finished successfully.
    virtual void OnSetupSuccess() = 0;
  };

  explicit DemoSetupController(Delegate* delegate);
  ~DemoSetupController() override;

  // Initiates online enrollment that registers and sets up the device in the
  // Demo Mode domain.
  void EnrollOnline();

  // Initiates offline enrollment that locks the device and sets up offline
  // policies required by Demo Mode. It requires no network connectivity since
  // and all setup will be done locally. The policy files will be loaded
  // from the |base_path|.
  void EnrollOffline(const base::FilePath& base_path);

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer:
  void OnDeviceEnrolled(const std::string& additional_token) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;
  void OnMultipleLicensesAvailable(
      const EnrollmentLicenseMap& licenses) override;

  void SetDeviceLocalAccountPolicyStoreForTest(policy::CloudPolicyStore* store);

 private:
  // Called when the checks of policy files for the offline demo mode is done.
  void OnOfflinePolicyFilesExisted(std::string* message, bool ok);

  // Called when the device local account policy for the offline demo mode is
  // loaded.
  void OnDeviceLocalAccountPolicyLoaded(base::Optional<std::string> blob);

  // Finish the flow with an error message.
  void SetupFailed(const std::string& message, bool fatal);

  // Clears the internal state.
  void Reset();

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  Delegate* delegate_ = nullptr;

  // The mode of the current enrollment flow.
  policy::EnrollmentConfig::Mode mode_ = policy::EnrollmentConfig::MODE_NONE;

  // The directory which contains the policy blob files for the offline
  // enrollment (i.e. device_policy and local_account_policy). Should be empty
  // on the online enrollment.
  base::FilePath policy_dir_;

  // The CloudPolicyStore for the device local account for the offline policy.
  policy::CloudPolicyStore* device_local_account_policy_store_ = nullptr;

  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;

  base::WeakPtrFactory<DemoSetupController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupController);
};

}  //  namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
