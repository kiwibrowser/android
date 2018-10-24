// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_TRANSPORT_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_TRANSPORT_LIST_VIEW_H_

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/webauthn/transport_list_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

// Represents a view that shows a list of transports available on a platform.
//
//   +----------------------------------+
//   | ICON1 | Transport 1 name     | > |
//   +----------------------------------+
//   | ICON2 | Transport 2 name     | > |
//   +----------------------------------+
//   | ICON3 | Transport 3 name     | > |
//   +----------------------------------+
//
class TransportListView : public views::View,
                          public views::ButtonListener,
                          public TransportListModel::Observer {
 public:
  // Interface that the client should implement to learn when the user clicks on
  // one of the items.
  class Delegate {
   public:
    virtual void OnListItemSelected(AuthenticatorTransport transport) = 0;
  };

  // The |model| and |delegate| must outlive this object, but the |delegate| may
  // be a nullptr.
  TransportListView(TransportListModel* model, Delegate* delegate);
  ~TransportListView() override;

 private:
  void AddViewForListItem(size_t index, AuthenticatorTransport transport);

  // TransportListModel::Observer:
  void OnModelDestroyed() override;
  void OnTransportAppended() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  TransportListModel* model_;  // Weak.
  Delegate* const delegate_;   // Weak, may be nullptr.

  DISALLOW_COPY_AND_ASSIGN(TransportListView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_TRANSPORT_LIST_VIEW_H_
