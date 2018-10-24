// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/last_window_closed_observer.h"

#include "ui/aura/env.h"
#include "ui/aura/window.h"

namespace keyboard_shortcut_viewer {

LastWindowClosedObserver::LastWindowClosedObserver(
    const base::RepeatingClosure& callback)
    : callback_(callback) {
  env_observer_.Add(aura::Env::GetInstanceDontCreate());
}

LastWindowClosedObserver::~LastWindowClosedObserver() = default;

void LastWindowClosedObserver::OnWindowInitialized(aura::Window* window) {
  window_observer_.Add(window);
}

void LastWindowClosedObserver::OnWindowDestroyed(aura::Window* window) {
  window_observer_.Remove(window);

  if (!window_observer_.IsObservingSources())
    callback_.Run();
}

}  // namespace keyboard_shortcut_viewer
