// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_controller.h"

#include <string>
#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/exo/shell_surface.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/base_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

CrostiniAppWindowShelfController::CrostiniAppWindowShelfController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

CrostiniAppWindowShelfController::~CrostiniAppWindowShelfController() {
  for (auto window : observed_window_to_startup_id_)
    window.first->RemoveObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
}

void CrostiniAppWindowShelfController::AddToShelf(aura::Window* window,
                                                  AppWindowBase* app_window) {
  ash::ShelfID shelf_id = app_window->shelf_id();
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(shelf_id);
  if (item_controller == nullptr) {
    auto controller =
        std::make_unique<AppWindowLauncherItemController>(shelf_id);
    item_controller = controller.get();
    if (!owner()->GetItem(shelf_id)) {
      owner()->CreateAppLauncherItem(std::move(controller),
                                     ash::STATUS_RUNNING);
    } else {
      owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                   std::move(controller));
      owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
    }
  }

  window->SetProperty(ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
  item_controller->AddWindow(app_window);
  app_window->SetController(item_controller);
}

void CrostiniAppWindowShelfController::RemoveFromShelf(
    aura::Window* window,
    AppWindowBase* app_window) {
  UnregisterAppWindow(app_window);

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(
          app_window->shelf_id());

  if (item_controller != nullptr && item_controller->window_count() == 0)
    owner()->CloseLauncherItem(item_controller->shelf_id());
}

void CrostiniAppWindowShelfController::ActiveUserChanged(
    const std::string& user_email) {
  for (auto& w : aura_window_to_app_window_) {
    if (MultiUserWindowManager::GetInstance()
            ->GetWindowOwner(w.first)
            .GetUserEmail() == user_email) {
      AddToShelf(w.first, w.second.get());
    } else {
      RemoveFromShelf(w.first, w.second.get());
    }
  }
}

void CrostiniAppWindowShelfController::OnWindowInitialized(
    aura::Window* window) {
  // An Crostini window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget. Tooltips, menus, and other kinds of transient
  // windows that can't activate are filtered out.
  if (window->type() != aura::client::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;
  if (wm::GetTransientParent(window) != nullptr)
    return;
  if (!widget->CanActivate())
    return;

  const std::string* startup_id = exo::ShellSurface::GetStartupId(window);
  if (startup_id == nullptr) {
    observed_window_to_startup_id_.emplace(window, std::string());
  } else {
    observed_window_to_startup_id_.emplace(window, *startup_id);
  }

  window->AddObserver(this);
}

void CrostiniAppWindowShelfController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  const std::string* startup_id = exo::ShellSurface::GetStartupId(window);
  if (startup_id == nullptr || startup_id->empty())
    return;
  if (*startup_id == observed_window_to_startup_id_.at(window))
    return;
  observed_window_to_startup_id_[window] = *startup_id;
  int64_t display_id =
      crostini_app_display_.GetDisplayIdForStartupId(*startup_id);
  if (display_id == display::kInvalidDisplayId)
    return;

  display::Display new_display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                             &new_display))
    return;
  display::Display old_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  // Adjust the window size and origin in proportion to the relative size of the
  // display.
  int old_width = old_display.bounds().width();
  int new_width = new_display.bounds().width();
  int old_height = old_display.bounds().height();
  int new_height = new_display.bounds().height();
  gfx::Rect old_bounds = window->bounds();
  gfx::Rect new_bounds(old_bounds.x() * new_width / old_width,
                       old_bounds.y() * new_height / old_height,
                       old_bounds.width() * new_width / old_width,
                       old_bounds.height() * new_height / old_height);

  // Transform the bounds in display to that in screen.
  gfx::Point new_origin = new_display.bounds().origin();
  new_origin.Offset(new_bounds.x(), new_bounds.y());
  new_bounds.set_origin(new_origin);
  window->SetBoundsInScreen(new_bounds, new_display);
}

void CrostiniAppWindowShelfController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (!visible)
    return;

  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  // Handle browser windows, such as the Crostini terminal.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser) {
    base::Optional<std::string> app_id =
        CrostiniAppIdFromAppName(browser->app_name());
    if (!app_id)
      return;
    RegisterAppWindow(window, app_id.value());
    return;
  }

  // Handle genuine Crostini app windows.
  const std::string* window_app_id =
      exo::ShellSurface::GetApplicationId(window);
  if (window_app_id == nullptr)
    return;

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          owner()->profile());
  const std::string& shelf_app_id = registry_service->GetCrostiniShelfAppId(
      *window_app_id, exo::ShellSurface::GetStartupId(window));
  // Non-crostini apps (i.e. arc++) are filtered out here.
  if (shelf_app_id.empty())
    return;

  // Prevent Crostini window from showing up after user switch.
  MultiUserWindowManager::GetInstance()->SetWindowOwner(
      window,
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  RegisterAppWindow(window, shelf_app_id);
}

void CrostiniAppWindowShelfController::RegisterAppWindow(
    aura::Window* window,
    const std::string& shelf_app_id) {
  const ash::ShelfID shelf_id(shelf_app_id);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  aura_window_to_app_window_[window] =
      std::make_unique<AppWindowBase>(shelf_id, widget);
  AppWindowBase* app_window = aura_window_to_app_window_[window].get();
  AddToShelf(window, app_window);
}

void CrostiniAppWindowShelfController::OnWindowDestroying(
    aura::Window* window) {
  auto it = observed_window_to_startup_id_.find(window);
  DCHECK(it != observed_window_to_startup_id_.end());
  observed_window_to_startup_id_.erase(it);
  window->RemoveObserver(this);

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;

  RemoveFromShelf(window, app_window_it->second.get());

  aura_window_to_app_window_.erase(app_window_it);
}

AppWindowLauncherItemController*
CrostiniAppWindowShelfController::ControllerForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return nullptr;

  AppWindowBase* app_window = app_window_it->second.get();

  if (app_window == nullptr)
    return nullptr;

  return app_window->controller();
}

void CrostiniAppWindowShelfController::UnregisterAppWindow(
    AppWindowBase* app_window) {
  if (!app_window)
    return;

  AppWindowLauncherItemController* controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);
  app_window->SetController(nullptr);
}

void CrostiniAppWindowShelfController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : aura_window_to_app_window_) {
    AppWindowBase* app_window = it.second.get();
    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    UnregisterAppWindow(it.second.get());
  }
}

void CrostiniAppWindowShelfController::OnAppLaunchRequested(
    const std::string& startup_id,
    int64_t display_id) {
  crostini_app_display_.Register(startup_id, display_id);
}
