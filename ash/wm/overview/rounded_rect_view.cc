// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/rounded_rect_view.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"

namespace ash {

RoundedRectView::RoundedRectView(int corner_radius, SkColor background_color)
    : corner_radius_(corner_radius), background_color_(background_color) {}

RoundedRectView::~RoundedRectView() = default;

void RoundedRectView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  SkScalar radius = SkIntToScalar(corner_radius_);
  const SkScalar kRadius[8] = {radius, radius, radius, radius,
                               radius, radius, radius, radius};
  SkPath path;
  gfx::Rect bounds(size());
  path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

  canvas->ClipPath(path, true);
  canvas->DrawColor(background_color_);
}

void RoundedRectView::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  SchedulePaint();
}

void RoundedRectView::SetCornerRadius(int radius) {
  if (corner_radius_ == radius)
    return;

  corner_radius_ = radius;
  SchedulePaint();
}

}  // namespace ash
