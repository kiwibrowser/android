// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_SHELL_CONTENT_STATE_H_
#define ASH_CONTENT_SHELL_CONTENT_STATE_H_

#include "ash/content/ash_with_content_export.h"
#include "ash/public/cpp/session_types.h"
#include "base/macros.h"


namespace aura {
class Window;
}

namespace content {
class BrowserContext;
}

namespace ash {

class ASH_WITH_CONTENT_EXPORT ShellContentState {
 public:
  static void SetInstance(ShellContentState* state);
  static ShellContentState* GetInstance();
  static void DestroyInstance();

  // Provides the embedder a way to return an active browser context for the
  // current user scenario. Default implementation here returns nullptr.
  virtual content::BrowserContext* GetActiveBrowserContext() = 0;

 protected:
  ShellContentState();
  virtual ~ShellContentState();

 private:
  static ShellContentState* instance_;

  DISALLOW_COPY_AND_ASSIGN(ShellContentState);
};

}  // namespace ash

#endif  // ASH_CONTENT_SHELL_CONTENT_STATE_H_
