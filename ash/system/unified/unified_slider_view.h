// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_

#include "ash/system/unified/top_shortcut_button.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

namespace ash {

class UnifiedSliderListener : public views::ButtonListener,
                              public views::SliderListener {
 public:
  // Instantiates UnifiedSliderView. The view will be onwed by views hierarchy.
  // The view should be always deleted after the controller is destructed.
  virtual views::View* CreateView() = 0;

  ~UnifiedSliderListener() override = default;
};

// A slider that ignores inputs.
// TODO(tetsui): Move to anonymous namespace.
class ReadOnlySlider : public views::Slider {
 public:
  ReadOnlySlider();

 private:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  DISALLOW_COPY_AND_ASSIGN(ReadOnlySlider);
};

// A button used in a slider row of UnifiedSystemTray. The button is togglable.
class UnifiedSliderButton : public TopShortcutButton {
 public:
  UnifiedSliderButton(views::ButtonListener* listener,
                      const gfx::VectorIcon& icon,
                      int accessible_name_id);
  ~UnifiedSliderButton() override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Change the toggle state.
  void SetToggled(bool toggled);

  // TopShortcutButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // Ture if the button is currently toggled.
  bool toggled_ = false;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSliderButton);
};

// Base view class of a slider row in UnifiedSystemTray. It has a button on the
// left side and a slider on the right side.
class UnifiedSliderView : public views::View {
 public:
  // If |readonly| is set, the slider will not accept any user events.
  UnifiedSliderView(UnifiedSliderListener* listener,
                    const gfx::VectorIcon& icon,
                    int accessible_name_id,
                    bool readonly = false);
  ~UnifiedSliderView() override;

  UnifiedSliderButton* button() { return button_; }
  views::Slider* slider() { return slider_; }

 private:
  // Unowned. Owned by views hierarchy.
  UnifiedSliderButton* const button_;
  views::Slider* const slider_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSliderView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
