// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"

// This is the counterpart to AppShimController in
// chrome/app/chrome_main_app_mode_mac.mm. The AppShimHost owns itself, and is
// destroyed when the app it corresponds to is closed or when the channel
// connected to the app shim is closed.
class AppShimHost : public chrome::mojom::AppShimHost,
                    public apps::AppShimHandler::Host {
 public:
  AppShimHost();
  ~AppShimHost() override;

  // Creates a new server-side mojo channel at |endpoint|, which should contain
  // a file descriptor of a channel created by an UnixDomainSocketAcceptor, and
  // begins listening for messages on it.
  void ServeChannel(mojo::PlatformChannelEndpoint endpoint);

 protected:
  void BindToRequest(chrome::mojom::AppShimHostRequest host_request);
  void ChannelError(uint32_t custom_reason, const std::string& description);

  // chrome::mojom::AppShimHost implementation.
  void LaunchApp(chrome::mojom::AppShimPtr app_shim_ptr,
                 const base::FilePath& profile_dir,
                 const std::string& app_id,
                 apps::AppShimLaunchType launch_type,
                 const std::vector<base::FilePath>& files) override;
  void FocusApp(apps::AppShimFocusType focus_type,
                const std::vector<base::FilePath>& files) override;
  void SetAppHidden(bool hidden) override;
  void QuitApp() override;

  // apps::AppShimHandler::Host overrides:
  void OnAppLaunchComplete(apps::AppShimLaunchResult result) override;
  void OnAppClosed() override;
  void OnAppHide() override;
  void OnAppUnhideWithoutActivation() override;
  void OnAppRequestUserAttention(apps::AppShimAttentionType type) override;
  base::FilePath GetProfilePath() const override;
  std::string GetAppId() const override;

  // Closes the channel and destroys the AppShimHost.
  void Close();

  mojo::IsolatedConnection mojo_connection_;
  chrome::mojom::AppShimPtr app_shim_;
  mojo::Binding<chrome::mojom::AppShimHost> host_binding_;
  std::string app_id_;
  base::FilePath profile_path_;
  bool initial_launch_finished_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(AppShimHost);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
