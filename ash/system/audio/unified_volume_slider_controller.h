// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_

#include "ash/system/unified/unified_slider_view.h"

namespace ash {

class UnifiedSystemTrayController;
class UnifiedVolumeView;

// Controller of a slider that can change audio volume.
class UnifiedVolumeSliderController : public UnifiedSliderListener {
 public:
  // |tray_controller| may be null if the volume slider is in slider bubble, not
  // main bubble.
  explicit UnifiedVolumeSliderController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedVolumeSliderController() override;

  // UnifiedSliderListener:
  views::View* CreateView() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  UnifiedSystemTrayController* const tray_controller_;
  UnifiedVolumeView* slider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedVolumeSliderController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
