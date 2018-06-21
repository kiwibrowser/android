// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_LAST_WINDOW_CLOSED_OBSERVER_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_LAST_WINDOW_CLOSED_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Env;
}  // namespace aura

namespace keyboard_shortcut_viewer {

// Monitors aura::Env and invokes a callback when the last known window is
// closed.
class LastWindowClosedObserver : public aura::EnvObserver,
                                 public aura::WindowObserver {
 public:
  explicit LastWindowClosedObserver(const base::RepeatingClosure& callback);
  ~LastWindowClosedObserver() override;

 private:
  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  base::RepeatingClosure callback_;

  ScopedObserver<aura::Env, aura::EnvObserver> env_observer_{this};
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(LastWindowClosedObserver);
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_LAST_WINDOW_CLOSED_OBSERVER_H_
