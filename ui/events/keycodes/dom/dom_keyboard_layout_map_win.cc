// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map_base.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

class DomKeyboardLayoutMapWin : public DomKeyboardLayoutMapBase {
 public:
  DomKeyboardLayoutMapWin();
  ~DomKeyboardLayoutMapWin() override;

 private:
  // ui::DomKeyboardLayoutMapBase implementation.
  uint32_t GetKeyboardLayoutCount() override;
  ui::DomKey GetDomKeyFromDomCodeForLayout(
      ui::DomCode dom_code,
      uint32_t keyboard_layout_index) override;

  // Set of keyboard layout handles provided by the operating system.
  // The handles stored do not need to be released when the vector is destroyed.
  std::vector<HKL> keyboard_layout_handles_;

  DISALLOW_COPY_AND_ASSIGN(DomKeyboardLayoutMapWin);
};

DomKeyboardLayoutMapWin::DomKeyboardLayoutMapWin() = default;

DomKeyboardLayoutMapWin::~DomKeyboardLayoutMapWin() = default;

uint32_t DomKeyboardLayoutMapWin::GetKeyboardLayoutCount() {
  keyboard_layout_handles_.clear();
  const size_t keyboard_layout_count = ::GetKeyboardLayoutList(0, nullptr);
  if (!keyboard_layout_count) {
    DPLOG(ERROR) << "GetKeyboardLayoutList failed: ";
    return false;
  }

  keyboard_layout_handles_.resize(keyboard_layout_count);
  const size_t copy_count = ::GetKeyboardLayoutList(
      keyboard_layout_handles_.size(), keyboard_layout_handles_.data());
  if (!copy_count) {
    DPLOG(ERROR) << "GetKeyboardLayoutList failed: ";
    return false;
  }
  DCHECK_EQ(keyboard_layout_count, copy_count);

  return keyboard_layout_handles_.size();
}

ui::DomKey DomKeyboardLayoutMapWin::GetDomKeyFromDomCodeForLayout(
    ui::DomCode dom_code,
    uint32_t keyboard_layout_index) {
  DCHECK_NE(dom_code, ui::DomCode::NONE);
  DCHECK_LT(keyboard_layout_index, keyboard_layout_handles_.size());

  HKL keyboard_layout = keyboard_layout_handles_[keyboard_layout_index];
  int32_t scan_code = ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  uint32_t virtual_key_code =
      MapVirtualKeyEx(scan_code, MAPVK_VSC_TO_VK_EX, keyboard_layout);
  if (!virtual_key_code) {
    if (GetLastError() != 0)
      DPLOG(ERROR) << "MapVirtualKeyEx failed: ";
    return ui::DomKey::NONE;
  }

  // Represents a keyboard state with all keys up (i.e. no keys pressed).
  BYTE keyboard_state[256] = {0};

  // ToUnicodeEx() return value indicates the category for the scan code
  // passed in for the keyboard layout provided.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms646322(v=vs.85).aspx
  wchar_t char_buffer[1] = {0};
  int key_type =
      ::ToUnicodeEx(virtual_key_code, scan_code, keyboard_state, char_buffer,
                    base::size(char_buffer), /*wFlags=*/0, keyboard_layout);

  if (key_type == 1)
    return ui::DomKey::FromCharacter(char_buffer[0]);
  if (key_type == -1)
    return ui::DomKey::DeadKeyFromCombiningCharacter(char_buffer[0]);

  return ui::DomKey::NONE;
}

}  // namespace

// static
base::flat_map<std::string, std::string> GenerateDomKeyboardLayoutMap() {
  return DomKeyboardLayoutMapWin().Generate();
}

}  // namespace ui
