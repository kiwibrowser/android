// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_SHORTCUT_VIEWER_APPLICATION_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_SHORTCUT_VIEWER_APPLICATION_H_

#include <memory>

#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace views {
class AuraInit;
}  // namespace views

namespace keyboard_shortcut_viewer {

class LastWindowClosedObserver;

// A mojo application that shows the keyboard shortcut viewer window.
class ShortcutViewerApplication
    : public service_manager::Service,
      public ui::InputDeviceEventObserver,
      public shortcut_viewer::mojom::ShortcutViewer {
 public:
  ShortcutViewerApplication();
  ~ShortcutViewerApplication() override;

  // Records a single trace event for shortcut viewer. chrome://tracing doesn't
  // allow selecting a trace event category for recording until the tracing
  // system has seen at least one event.
  static void RegisterForTraceEvents();

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // ui::InputDeviceEventObserver:
  void OnDeviceListsComplete() override;

  // shortcut_viewer::mojom:ShortcutViewer:
  void Toggle(base::TimeTicks user_gesture_time) override;

  void AddBinding(shortcut_viewer::mojom::ShortcutViewerRequest request);

  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<LastWindowClosedObserver> last_window_closed_observer_;

  service_manager::BinderRegistry registry_;

  mojo::Binding<shortcut_viewer::mojom::ShortcutViewer>
      shortcut_viewer_binding_;

  // Timestamp of the user gesture (e.g. Ctrl-Shift-/ keystroke) that triggered
  // showing the window. Used for metrics.
  base::TimeTicks user_gesture_time_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutViewerApplication);
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_SHORTCUT_VIEWER_APPLICATION_H_
