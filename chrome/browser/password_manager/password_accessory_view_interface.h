// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_VIEW_INTERFACE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_VIEW_INTERFACE_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "url/gurl.h"

class PasswordAccessoryController;

// The interface for creating and controlling a view for the password accessory.
// The view gets data from a given |PasswordAccessoryController| and forwards
// any request (like filling a suggestion) back to the controller.
class PasswordAccessoryViewInterface {
 public:
  // Represents an item that will be shown in the bottom sheet below a keyboard
  // accessory.
  struct AccessoryItem {
    // Maps to its java counterpart PasswordAccessoryModel.Item.Type.
    enum class Type { LABEL = 1, SUGGESTION = 2 };
    // The |text| is caption of the item and what will be filled if selected.
    base::string16 text;
    // The |content_description| is used for accessibility on displayed items.
    base::string16 content_description;
    // If true, the item contains a password (i.e. it's text should be masked).
    bool is_password;
    // Visual appearance and whether an item is clickable depend on this.
    Type itemType;

    AccessoryItem(const base::string16& text,
                  const base::string16& content_description,
                  bool is_password,
                  Type itemType)
        : text(text),
          content_description(content_description),
          is_password(is_password),
          itemType(itemType) {}
  };

  // Called with items that should replace all existing items in the accessory
  // sheet. The |origin| will be used to let the user know to which site the
  // passwords belong and the |items| are the labels and actions that allow the
  // filling.
  virtual void OnItemsAvailable(const GURL& origin,
                                const std::vector<AccessoryItem>& items) = 0;

  virtual ~PasswordAccessoryViewInterface() = default;

 private:
  friend class PasswordAccessoryController;
  // Factory function used to create a concrete instance of this view.
  static std::unique_ptr<PasswordAccessoryViewInterface> Create(
      PasswordAccessoryController* controller);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_VIEW_INTERFACE_H_
