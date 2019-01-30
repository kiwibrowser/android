// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shell_state_client.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace {

ShellStateClient* g_shell_state_client = nullptr;

}  // namespace

ShellStateClient::ShellStateClient()
    : binding_(this), display_id_for_new_windows_(display::kInvalidDisplayId) {
  DCHECK(!g_shell_state_client);
  g_shell_state_client = this;
}

ShellStateClient::~ShellStateClient() {
  DCHECK_EQ(this, g_shell_state_client);
  g_shell_state_client = nullptr;
}

void ShellStateClient::Init() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_state_ptr_);
  BindAndAddClient();
}

void ShellStateClient::InitForTesting(ash::mojom::ShellStatePtr shell_state) {
  shell_state_ptr_ = std::move(shell_state);
  BindAndAddClient();
}

// static
ShellStateClient* ShellStateClient::Get() {
  return g_shell_state_client;
}

void ShellStateClient::SetDisplayIdForNewWindows(int64_t display_id) {
  display_id_for_new_windows_ = display_id;
}

void ShellStateClient::FlushForTesting() {
  shell_state_ptr_.FlushForTesting();
}

void ShellStateClient::BindAndAddClient() {
  ash::mojom::ShellStateClientPtr client_ptr;
  binding_.Bind(mojo::MakeRequest(&client_ptr));
  shell_state_ptr_->AddClient(std::move(client_ptr));
}

// static
display::Display WindowSizer::GetDisplayForNewWindow(const gfx::Rect& bounds) {
  display::Screen* screen = display::Screen::GetScreen();
  // May be null in unit tests.
  if (g_shell_state_client) {
    // Prefer the display where the user last activated any window.
    const int64_t id = g_shell_state_client->display_id_for_new_windows();
    display::Display display;
    if (screen->GetDisplayWithDisplayId(id, &display))
      return display;
  }

  // Otherwise find the display that best matches the bounds.
  return screen->GetDisplayMatching(bounds);
}
