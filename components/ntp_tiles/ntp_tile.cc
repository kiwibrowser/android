// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/ntp_tile.h"

namespace ntp_tiles {

NTPTile::NTPTile()
    : title_source(TileTitleSource::UNKNOWN),
      source(TileSource::TOP_SITES),
      has_fallback_style(false),
      fallback_background_color(SK_ColorBLACK),
      fallback_text_color(SK_ColorBLACK) {}

NTPTile::NTPTile(const NTPTile&) = default;

NTPTile::~NTPTile() {}

bool operator==(const NTPTile& a, const NTPTile& b) {
  return (a.title == b.title) && (a.url == b.url) && (a.source == b.source) &&
         (a.title_source == b.title_source) &&
         (a.whitelist_icon_path == b.whitelist_icon_path) &&
         (a.thumbnail_url == b.thumbnail_url) &&
         (a.favicon_url == b.favicon_url) &&
         (a.has_fallback_style == b.has_fallback_style) &&
         (a.fallback_background_color == b.fallback_background_color) &&
         (a.fallback_text_color == b.fallback_text_color);
}

bool operator!=(const NTPTile& a, const NTPTile& b) {
  return !(a == b);
}

}  // namespace ntp_tiles
