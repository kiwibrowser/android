// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_

#include <list>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace chromeos {

// Tracks global demo session state. For example, whether the demo session has
// started, and whether the demo session offline resources have been loaded.
class DemoSession {
 public:
  // Whether the device is set up to run demo sessions.
  static bool IsDeviceInDemoMode();
  static void SetDeviceInDemoModeForTesting(bool in_demo_mode);

  // If the device is set up to run in demo mode, marks demo session as started,
  // and requests load of demo session resources.
  // Creates global DemoSession instance if required.
  static DemoSession* StartIfInDemoMode();

  // Requests load of demo session resources, without marking the demo session
  // as started. Creates global DemoSession instance if required.
  static void PreloadOfflineResourcesIfInDemoMode();

  // Deletes the global DemoSession instance if it was previously created.
  static void ShutDownIfInitialized();

  // Gets the global demo session instance. Returns nullptr if the DemoSession
  // instance has not yet been initialized (either by calling
  // StartIfInDemoMode() or PreloadOfflineResourcesIfInDemoMode()).
  static DemoSession* Get();

  // Ensures that the load of offline demo session resources is requested.
  // |load_callback| will be run once the offline resource load finishes.
  void EnsureOfflineResourcesLoaded(base::OnceClosure load_callback);

  // Gets the path of the image containing demo session Android apps. The path
  // will be set when the offline resources get loaded.
  base::FilePath GetDemoAppsPath() const;

  bool started() const { return started_; }
  bool offline_resources_loaded() const { return offline_resources_loaded_; }

 private:
  DemoSession();
  ~DemoSession();

  // Callback for the image loader request to load offline demo mode resources.
  // |mount_path| is the path at which the resources were loaded.
  void OnOfflineResourcesLoaded(base::Optional<base::FilePath> mounted_path);

  bool started_ = false;

  bool offline_resources_load_requested_ = false;
  bool offline_resources_loaded_ = false;

  // Path at which offline demo mode resources were loaded.
  base::FilePath offline_resources_path_;

  // List of pending callbacks passed to EnsureOfflineResourcesLoaded().
  std::list<base::OnceClosure> offline_resources_load_callbacks_;

  base::WeakPtrFactory<DemoSession> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoSession);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_
