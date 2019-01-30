// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/crostini/crostini_app_launch_observer.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

constexpr char kCrostiniAppLaunchHistogram[] = "Crostini.AppLaunch";
constexpr char kCrostiniAppNamePrefix[] = "_crostini_";

// If true then override IsCrostiniUIAllowedForProfile and related methods to
// turn on Crostini.
bool g_crostini_ui_allowed_for_testing = false;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniAppLaunchAppType {
  // An app which isn't in the CrostiniAppRegistry. This shouldn't happen.
  kUnknownApp = 0,

  // The main terminal app.
  kTerminal = 1,

  // An app for which there is something in the CrostiniAppRegistry.
  kRegisteredApp = 2,

  kCount
};

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type) {
  base::UmaHistogramEnumeration(kCrostiniAppLaunchHistogram, app_type,
                                CrostiniAppLaunchAppType::kCount);
}

void OnLaunchFailed(const std::string& app_id) {
  // Remove the spinner so it doesn't stay around forever.
  // TODO(timloh): Consider also displaying a notification of some sort.
  ChromeLauncherController* chrome_controller =
      ChromeLauncherController::instance();
  DCHECK(chrome_controller);
  chrome_controller->GetShelfSpinnerController()->Close(app_id);
}

void OnCrostiniRestarted(const std::string& app_id,
                         Browser* browser,
                         base::OnceClosure callback,
                         crostini::ConciergeClientResult result) {
  if (result != crostini::ConciergeClientResult::SUCCESS) {
    OnLaunchFailed(app_id);
    if (browser && browser->window())
      browser->window()->Close();
    return;
  }
  std::move(callback).Run();
}

void OnContainerApplicationLaunched(const std::string& app_id,
                                    crostini::ConciergeClientResult result) {
  if (result != crostini::ConciergeClientResult::SUCCESS)
    OnLaunchFailed(app_id);
}

Browser* CreateTerminal(const AppLaunchParams& launch_params,
                        const GURL& vsh_in_crosh_url) {
  return crostini::CrostiniManager::GetInstance()->CreateContainerTerminal(
      launch_params, vsh_in_crosh_url);
}

void ShowTerminal(const AppLaunchParams& launch_params,
                  const GURL& vsh_in_crosh_url,
                  Browser* browser) {
  crostini::CrostiniManager::GetInstance()->ShowContainerTerminal(
      launch_params, vsh_in_crosh_url, browser);
}

void LaunchContainerApplication(
    Profile* profile,
    const std::string& app_id,
    crostini::CrostiniRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<std::string>& files) {
  ChromeLauncherController* chrome_launcher_controller =
      ChromeLauncherController::instance();
  DCHECK_NE(chrome_launcher_controller, nullptr);
  CrostiniAppLaunchObserver* observer =
      chrome_launcher_controller->crostini_app_window_shelf_controller();
  DCHECK_NE(observer, nullptr);
  observer->OnAppLaunchRequested(registration.DesktopFileId(), display_id);
  crostini::CrostiniManager::GetInstance()->LaunchContainerApplication(
      profile, registration.VmName(), registration.ContainerName(),
      registration.DesktopFileId(), files,
      base::BindOnce(OnContainerApplicationLaunched, app_id));
}

}  // namespace

void SetCrostiniUIAllowedForTesting(bool enabled) {
  g_crostini_ui_allowed_for_testing = enabled;
}

bool IsCrostiniAllowedForProfile(Profile* profile) {
  if (g_crostini_ui_allowed_for_testing) {
    return true;
  }
  if (profile && (profile->IsChild() || profile->IsLegacySupervised())) {
    return false;
  }
  return virtual_machines::AreVirtualMachinesAllowedByVersionAndChannel() &&
         virtual_machines::AreVirtualMachinesAllowedByPolicy() &&
         base::FeatureList::IsEnabled(features::kCrostini);
}

bool IsCrostiniUIAllowedForProfile(Profile* profile) {
  if (g_crostini_ui_allowed_for_testing) {
    return true;
  }
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }

  return IsCrostiniAllowedForProfile(profile) &&
         base::FeatureList::IsEnabled(features::kExperimentalCrostiniUI);
}

bool IsCrostiniEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id) {
  LaunchCrostiniApp(profile, app_id, display_id, std::vector<std::string>());
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<std::string>& files) {
  auto* crostini_manager = crostini::CrostiniManager::GetInstance();
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    LOG(ERROR) << "LaunchCrostiniApp called with an unknown app_id: " << app_id;
    return;
  }

  // Store these as we move |registration| into LaunchContainerApplication().
  const std::string vm_name = registration->VmName();
  const std::string container_name = registration->ContainerName();

  base::OnceClosure launch_closure;
  Browser* browser = nullptr;
  if (app_id == kCrostiniTerminalId) {
    DCHECK(files.empty());
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kTerminal);

    if (!crostini_manager->IsCrosTerminaInstalled() ||
        !IsCrostiniEnabled(profile)) {
      ShowCrostiniInstallerView(profile, CrostiniUISurface::kAppList);
      return;
    }

    GURL vsh_in_crosh_url = crostini::CrostiniManager::GenerateVshInCroshUrl(
        profile, vm_name, container_name);
    AppLaunchParams launch_params =
        crostini::CrostiniManager::GenerateTerminalAppLaunchParams(profile);
    // Create the terminal here so it's created in the right display. If the
    // browser creation is delayed into the callback the root window for new
    // windows setting can be changed due to the launcher or shelf dismissal.
    Browser* browser = CreateTerminal(launch_params, vsh_in_crosh_url);
    launch_closure =
        base::BindOnce(&ShowTerminal, launch_params, vsh_in_crosh_url, browser);
  } else {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);
    launch_closure =
        base::BindOnce(&LaunchContainerApplication, profile, app_id,
                       std::move(*registration), display_id, std::move(files));
  }

  // Update the last launched time.
  registry_service->AppLaunched(app_id);

  // Show a spinner as it may take a while for the app window to appear.
  ChromeLauncherController* chrome_controller =
      ChromeLauncherController::instance();
  DCHECK(chrome_controller);
  chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
      app_id, std::make_unique<ShelfSpinnerItemController>(app_id));

  crostini_manager->RestartCrostini(
      profile, vm_name, container_name,
      base::BindOnce(OnCrostiniRestarted, app_id, browser,
                     std::move(launch_closure)));
}

std::string CryptohomeIdForProfile(Profile* profile) {
  std::string id = chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  // Empty id means we're running in a test.
  return id.empty() ? "test" : id;
}

std::string ContainerUserNameForProfile(Profile* profile) {
  // Get rid of the @domain.name in the profile user name (an email address).
  std::string container_username = profile->GetProfileUserName();
  if (container_username.find('@') != std::string::npos) {
    // gaia::CanonicalizeEmail CHECKs its argument contains'@'.
    container_username = gaia::CanonicalizeEmail(container_username);
    // |container_username| may have changed, so we have to find again.
    return container_username.substr(0, container_username.find('@'));
  }
  return container_username;
}

base::FilePath HomeDirectoryForProfile(Profile* profile) {
  return base::FilePath("/home/" + ContainerUserNameForProfile(profile));
}

std::string AppNameFromCrostiniAppId(const std::string& id) {
  return kCrostiniAppNamePrefix + id;
}

base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name) {
  if (!base::StartsWith(app_name, kCrostiniAppNamePrefix,
                        base::CompareCase::SENSITIVE)) {
    return base::nullopt;
  }
  return app_name.substr(strlen(kCrostiniAppNamePrefix));
}
