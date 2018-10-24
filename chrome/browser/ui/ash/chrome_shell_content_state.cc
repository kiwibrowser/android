// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_content_state.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

ChromeShellContentState* g_chrome_shell_content_state_instance = nullptr;

}  // namespace

// static
ChromeShellContentState* ChromeShellContentState::GetInstance() {
  DCHECK(g_chrome_shell_content_state_instance);
  return g_chrome_shell_content_state_instance;
}

ChromeShellContentState::ChromeShellContentState() {
  DCHECK(!g_chrome_shell_content_state_instance);
  g_chrome_shell_content_state_instance = this;
}

ChromeShellContentState::~ChromeShellContentState() {
  DCHECK_EQ(this, g_chrome_shell_content_state_instance);
  g_chrome_shell_content_state_instance = nullptr;
}

content::BrowserContext* ChromeShellContentState::GetActiveBrowserContext() {
  return ProfileManager::GetActiveUserProfile();
}
