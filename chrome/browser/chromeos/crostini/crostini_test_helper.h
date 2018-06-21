// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_HELPER_H_

#include <string>

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"

class Profile;

namespace crostini {

class CrostiniRegistryService;

// This class is used to help test Crostini app integration by providing a
// simple interface to add, update, and remove apps from the registry.
class CrostiniTestHelper {
 public:
  // For convenience, instantiating this enables Crostini and also calls
  // SetCrostiniUIAllowedForTesting(true). The destructor resets these.
  explicit CrostiniTestHelper(Profile*);
  ~CrostiniTestHelper();

  // Creates the apps named "dummy1" and "dummy2" in the default container.
  void SetupDummyApps();
  // Returns the |i|th app from the current list of apps.
  vm_tools::apps::App GetApp(int i);
  // Adds an app in the default container. Replaces an existing app with the
  // same desktop file id if one exists.
  void AddApp(const vm_tools::apps::App& app);
  // Removes the |i|th app from the current list of apps.
  void RemoveApp(int i);

  // Set/unset the the CrostiniEnabled pref
  static void EnableCrostini(Profile* profile);
  static void DisableCrostini(Profile* profile);

  // Returns the app id that the registry would use for the given desktop file.
  static std::string GenerateAppId(
      const std::string& desktop_file_id,
      const std::string& vm_name = kCrostiniDefaultVmName,
      const std::string& container_name = kCrostiniDefaultContainerName);

  // Returns an App with the desktop file id and default name as provided.
  static vm_tools::apps::App BasicApp(const std::string& desktop_file_id,
                                      const std::string& name = "");

  // Returns an ApplicationList with a single desktop file.
  static vm_tools::apps::ApplicationList BasicAppList(
      const std::string& desktop_file_id,
      const std::string& vm_name,
      const std::string& container_name);

 private:
  void UpdateRegistry();

  Profile* profile_;
  vm_tools::apps::ApplicationList current_apps_;
  CrostiniRegistryService* registry_service_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_HELPER_H_
