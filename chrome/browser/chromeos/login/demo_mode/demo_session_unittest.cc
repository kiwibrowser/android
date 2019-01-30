// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kOfflineResourcesComponent[] = "demo_mode_resources";
constexpr char kTestDemoModeResourcesMountPoint[] =
    "/run/imageloader/demo_mode_resources";
constexpr char kDemoAppsImageFile[] = "android_demo_apps.squash";

void SetBoolean(bool* value) {
  *value = true;
}

class TestImageLoaderClient : public FakeImageLoaderClient {
 public:
  TestImageLoaderClient() = default;
  ~TestImageLoaderClient() override = default;

  const std::list<std::string>& pending_loads() const { return pending_loads_; }

  // FakeImageLoaderClient:
  void LoadComponentAtPath(
      const std::string& name,
      const base::FilePath& path,
      DBusMethodCallback<base::FilePath> callback) override {
    ASSERT_FALSE(path.empty());

    components_[name].source = path;
    components_[name].load_callbacks.emplace_back(std::move(callback));
    pending_loads_.push_back(name);
  }

  bool FinishComponentLoad(const std::string& component_name,
                           const base::FilePath& mount_point) {
    if (pending_loads_.empty() || pending_loads_.front() != component_name)
      return false;
    pending_loads_.pop_front();

    components_[component_name].loaded = true;
    RunPendingLoadCallback(component_name, mount_point);
    return true;
  }

  bool FailComponentLoad(const std::string& component_name) {
    if (pending_loads_.empty() || pending_loads_.front() != component_name)
      return false;
    pending_loads_.pop_front();
    RunPendingLoadCallback(component_name, base::nullopt);
    return true;
  }

  bool ComponentLoadedFromPath(const std::string& name,
                               const base::FilePath& file_path) const {
    const auto& component = components_.find(name);
    if (component == components_.end())
      return false;
    return component->second.loaded && component->second.source == file_path;
  }

 private:
  struct ComponentInfo {
    ComponentInfo() = default;
    ~ComponentInfo() = default;

    base::FilePath source;
    bool loaded = false;
    std::list<DBusMethodCallback<base::FilePath>> load_callbacks;
  };

  void RunPendingLoadCallback(const std::string& component_name,
                              base::Optional<base::FilePath> mount_point) {
    DBusMethodCallback<base::FilePath> callback =
        std::move(components_[component_name].load_callbacks.front());
    components_[component_name].load_callbacks.pop_front();

    std::move(callback).Run(std::move(mount_point));
  }

  // Map containing known components.
  std::map<std::string, ComponentInfo> components_;

  // List of components whose load has been requested.
  std::list<std::string> pending_loads_;

  DISALLOW_COPY_AND_ASSIGN(TestImageLoaderClient);
};

}  // namespace

class DemoSessionTest : public testing::Test {
 public:
  DemoSessionTest() = default;
  ~DemoSessionTest() override = default;

  void SetUp() override {
    chromeos::DemoSession::SetDeviceInDemoModeForTesting(true);
    auto image_loader_client = std::make_unique<TestImageLoaderClient>();
    image_loader_client_ = image_loader_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetImageLoaderClient(
        std::move(image_loader_client));
  }

  void TearDown() override {
    DemoSession::ShutDownIfInitialized();
    image_loader_client_ = nullptr;
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  // Points to the image loader client passed to the test DBusTestManager.
  TestImageLoaderClient* image_loader_client_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoSessionTest);
};

TEST_F(DemoSessionTest, StartForDeviceInDemoMode) {
  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_EQ(demo_session, DemoSession::Get());
}

TEST_F(DemoSessionTest, StartInitiatesOfflineResourcesLoad) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  EXPECT_FALSE(demo_session->offline_resources_loaded());
  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

TEST_F(DemoSessionTest, StartForDemoDeviceNotInDemoMode) {
  DemoSession::SetDeviceInDemoModeForTesting(false);
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(DemoSession::StartIfInDemoMode());
  EXPECT_FALSE(DemoSession::Get());

  EXPECT_EQ(std::list<std::string>(), image_loader_client_->pending_loads());
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfInDemoMode) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);
  EXPECT_FALSE(demo_session->started());

  EXPECT_FALSE(demo_session->offline_resources_loaded());
  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  EXPECT_FALSE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfNotInDemoMode) {
  DemoSession::SetDeviceInDemoModeForTesting(false);
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_EQ(std::list<std::string>(), image_loader_client_->pending_loads());
}

TEST_F(DemoSessionTest, ShutdownResetsInstance) {
  ASSERT_TRUE(DemoSession::StartIfInDemoMode());
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, ShutdownAfterPreload) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, StartDemoSessionWhilePreloadingResources) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();

  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());

  EXPECT_FALSE(demo_session->offline_resources_loaded());
  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

TEST_F(DemoSessionTest, StartDemoSessionAfterPreloadingResources) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());

  EXPECT_EQ(std::list<std::string>(), image_loader_client_->pending_loads());
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterStart) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));

  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterOfflineResourceLoad) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));
  EXPECT_EQ(std::list<std::string>(), image_loader_client_->pending_loads());

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterPreload) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));

  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

TEST_F(DemoSessionTest, MultipleEnsureOfflineResourcesLoaded) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  bool first_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &first_callback_called));

  bool second_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &second_callback_called));

  bool third_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &third_callback_called));

  EXPECT_FALSE(first_callback_called);
  EXPECT_FALSE(second_callback_called);
  EXPECT_FALSE(third_callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  EXPECT_EQ(std::list<std::string>({kOfflineResourcesComponent}),
            image_loader_client_->pending_loads());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(image_loader_client_->FinishComponentLoad(
      kOfflineResourcesComponent, component_mount_point));

  EXPECT_TRUE(first_callback_called);
  EXPECT_TRUE(second_callback_called);
  EXPECT_TRUE(third_callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

}  // namespace chromeos
