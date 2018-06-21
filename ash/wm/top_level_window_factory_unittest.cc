// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/top_level_window_factory.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/window_manager.h"
#include "ash/window_manager_service.h"
#include "ash/wm/window_properties.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/ws2/window_tree_test_helper.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/test/gfx_util.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

int64_t GetDisplayId(aura::Window* window) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

}  // namespace

class TopLevelWindowFactoryTest : public AshTestBase {
 public:
  TopLevelWindowFactoryTest() = default;
  ~TopLevelWindowFactoryTest() override = default;

  aura::Window* CreateFullscreenTestWindow(int64_t display_id) {
    std::map<std::string, std::vector<uint8_t>> properties;
    properties[ui::mojom::WindowManager::kShowState_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<aura::PropertyConverter::PrimitiveType>(
                ui::mojom::ShowState::FULLSCREEN));
    if (display_id != display::kInvalidDisplayId) {
      properties[ui::mojom::WindowManager::kDisplayId_InitProperty] =
          mojo::ConvertTo<std::vector<uint8_t>>(display_id);
    }
    properties[ui::mojom::WindowManager::kWindowType_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int32_t>(ui::mojom::WindowType::WINDOW));
    aura::Window* window = GetWindowTreeTestHelper()->NewTopLevelWindow(
        mojo::MapToFlatMap(std::move(properties)));
    window->Show();
    return window;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TopLevelWindowFactoryTest);
};

TEST_F(TopLevelWindowFactoryTest, CreateFullscreenWindow) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ::wm::SetWindowFullscreen(window.get(), true);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  EXPECT_EQ(root_window->bounds(), window->bounds());
}

TEST_F(TopLevelWindowFactoryTest, IsWindowShownInCorrectDisplay) {
  UpdateDisplay("400x400,400x400");
  EXPECT_NE(GetPrimaryDisplay().id(), GetSecondaryDisplay().id());

  std::unique_ptr<aura::Window> window_primary_display(
      CreateFullscreenTestWindow(GetPrimaryDisplay().id()));
  std::unique_ptr<aura::Window> window_secondary_display(
      CreateFullscreenTestWindow(GetSecondaryDisplay().id()));

  EXPECT_EQ(GetPrimaryDisplay().id(),
            GetDisplayId(window_primary_display.get()));
  EXPECT_EQ(GetSecondaryDisplay().id(),
            GetDisplayId(window_secondary_display.get()));
}

TEST_F(TopLevelWindowFactoryTest, UnknownWindowTypeReturnsNull) {
  EXPECT_FALSE(GetWindowTreeTestHelper()->NewTopLevelWindow());
}

TEST_F(TopLevelWindowFactoryTest, CreateTopLevelWindow) {
  const gfx::Rect bounds(1, 2, 124, 345);
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kBounds_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(bounds);
  properties[ui::mojom::WindowManager::kResizeBehavior_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<aura::PropertyConverter::PrimitiveType>(
              ui::mojom::kResizeBehaviorCanResize |
              ui::mojom::kResizeBehaviorCanMaximize |
              ui::mojom::kResizeBehaviorCanMinimize));
  properties[ui::mojom::WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ui::mojom::WindowType::WINDOW));
  aura::Window* window = GetWindowTreeTestHelper()->NewTopLevelWindow(
      mojo::MapToFlatMap(std::move(properties)));
  ASSERT_TRUE(window->parent());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(bounds, window->bounds());
  EXPECT_EQ(WidgetCreationType::FOR_CLIENT,
            window->GetProperty(kWidgetCreationTypeKey));
  EXPECT_FALSE(window->IsVisible());
}

TEST_F(TopLevelWindowFactoryTest, CreateUnfocusableTopLevelWindow) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kFocusable_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(false);
  properties[ui::mojom::WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ui::mojom::WindowType::WINDOW));
  aura::Window* window = GetWindowTreeTestHelper()->NewTopLevelWindow(
      mojo::MapToFlatMap(std::move(properties)));
  ASSERT_TRUE(window);
  window->Show();
  // The window should not be focusable as kFocusable_InitProperty was supplied
  // with a value of false.
  EXPECT_FALSE(window->CanFocus());
}

TEST_F(TopLevelWindowFactoryTest, CreateUnfocusablePopupWindow) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kFocusable_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(false);
  properties[ui::mojom::WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ui::mojom::WindowType::POPUP));
  aura::Window* window = GetWindowTreeTestHelper()->NewTopLevelWindow(
      mojo::MapToFlatMap(std::move(properties)));
  ASSERT_TRUE(window);
  window->Show();
  // The window should not be focusable as kFocusable_InitProperty was supplied
  // with a value of false.
  EXPECT_FALSE(window->CanFocus());
}

}  // namespace ash
