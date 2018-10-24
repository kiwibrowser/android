// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/transport_list_view.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"

namespace {

// Gets the message ID for the human readable name of |transport|.
int GetHumanReadableTransportNameMessageId(AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kBluetoothLowEnergy:
      return IDS_WEBAUTHN_TRANSPORT_BLE;
    case AuthenticatorTransport::kNearFieldCommunication:
      return IDS_WEBAUTHN_TRANSPORT_NFC;
    case AuthenticatorTransport::kUsb:
      return IDS_WEBAUTHN_TRANSPORT_USB;
    case AuthenticatorTransport::kInternal:
      return IDS_WEBAUTHN_TRANSPORT_INTERNAL;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      return IDS_WEBAUTHN_TRANSPORT_CABLE;
  }
  NOTREACHED();
  return 0;
}

// Gets the vector icon depicting the given |transport|.
const gfx::VectorIcon& GetTransportVectorIcon(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kBluetoothLowEnergy:
      return kBluetoothIcon;
    case AuthenticatorTransport::kNearFieldCommunication:
      return kNfcIcon;
    case AuthenticatorTransport::kUsb:
      return vector_icons::kUsbIcon;
    case AuthenticatorTransport::kInternal:
      return kFingerprintIcon;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      return kSmartphoneIcon;
  }
  NOTREACHED();
  return kFingerprintIcon;
}

// Creates, for a given transport, the corresponding row in the transport list,
// containing an icon, a human-readable name, and a chevron at the right:
//
//   +--------------------------------+
//   | ICON | Transport name      | > |
//   +--------------------------------+
//
std::unique_ptr<HoverButton> CreateTransportListItemView(
    AuthenticatorTransport transport,
    views::ButtonListener* listener) {
  // Derive the icon color from the text color of an enabled label.
  auto color_reference_label = std::make_unique<views::Label>(
      base::string16(), CONTEXT_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY);
  const SkColor icon_color = color_utils::DeriveDefaultIconColor(
      color_reference_label->enabled_color());

  constexpr int kTransportIconSize = 24;
  auto transport_image = std::make_unique<views::ImageView>();
  transport_image->SetImage(gfx::CreateVectorIcon(
      GetTransportVectorIcon(transport), kTransportIconSize, icon_color));

  base::string16 transport_name = l10n_util::GetStringUTF16(
      GetHumanReadableTransportNameMessageId(transport));

  auto chevron_image = std::make_unique<views::ImageView>();
  chevron_image->SetImage(
      gfx::CreateVectorIcon(views::kSubmenuArrowIcon, icon_color));

  auto hover_button = std::make_unique<HoverButton>(
      listener, std::move(transport_image), transport_name,
      base::string16() /* subtitle */, std::move(chevron_image));
  hover_button->set_tag(static_cast<int>(transport));
  return hover_button;
}

void AddSeparatorAsChild(views::View* view) {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(gfx::kGoogleGrey900);
  view->AddChildView(separator.release());
}

}  // namespace

// TransportListView ---------------------------------------------------------

TransportListView::TransportListView(TransportListModel* model,
                                     Delegate* delegate)
    : model_(model), delegate_(delegate) {
  DCHECK(model_);
  model_->AddObserver(this);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0));
  AddSeparatorAsChild(this);
  for (size_t index = 0; index < model_->transports().size(); ++index) {
    const std::vector<AuthenticatorTransport>& transports =
        model_->transports();
    AddViewForListItem(index, transports[index]);
  }
}

TransportListView::~TransportListView() {
  if (model_) {
    model_->RemoveObserver(this);
    model_ = nullptr;
  }
}

void TransportListView::AddViewForListItem(size_t index,
                                           AuthenticatorTransport transport) {
  std::unique_ptr<HoverButton> list_item_view =
      CreateTransportListItemView(transport, this);
  AddChildView(list_item_view.release());
  AddSeparatorAsChild(this);
}

void TransportListView::OnModelDestroyed() {
  model_ = nullptr;
}

void TransportListView::OnTransportAppended() {
  DCHECK(!model_->transports().empty());
  AddViewForListItem(model_->transports().size() - 1,
                     model_->transports().back());

  // TODO(engedy): The enclosing dialog may also need to be resized, similarly
  // to what is done in AuthenticatorRequestDialogView::ReplaceSheetWith().
  Layout();
}

void TransportListView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (!delegate_)
    return;

  auto transport = static_cast<AuthenticatorTransport>(sender->tag());
  delegate_->OnListItemSelected(transport);
}
