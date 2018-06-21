// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button_border.h"

AvatarToolbarButton::AvatarToolbarButton(Profile* profile,
                                         views::ButtonListener* listener)
    : ToolbarButton(profile, listener, nullptr),
      profile_(profile),
#if !defined(OS_CHROMEOS)
      error_controller_(this, profile_),
#endif  // !defined(OS_CHROMEOS)
      profile_observer_(this),
      cookie_manager_service_observer_(this),
      account_tracker_service_observer_(this) {
  profile_observer_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

  if (!IsIncognito() && !profile_->IsGuestSession()) {
    cookie_manager_service_observer_.Add(
        GaiaCookieManagerServiceFactory::GetForProfile(profile_));
    account_tracker_service_observer_.Add(
        AccountTrackerServiceFactory::GetForProfile(profile_));
  }

  SetImageAlignment(HorizontalAlignment::ALIGN_CENTER,
                    VerticalAlignment::ALIGN_MIDDLE);

  // In non-touch mode we use a larger-than-normal icon size for avatars as 16dp
  // is hard to read for user avatars. This constant is correspondingly smaller
  // than GetLayoutInsets(TOOLBAR_BUTTON).
  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    SetBorder(views::CreateEmptyBorder(gfx::Insets(4)));

  // Activate on press for left-mouse-button only to mimic other MenuButtons
  // without drag-drop actions (specifically the adjacent browser menu).
  set_notify_action(Button::NOTIFY_ON_PRESS);
  set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON);

  set_tag(IDC_SHOW_AVATAR_MENU);
  set_id(VIEW_ID_AVATAR_BUTTON);

  Init();

#if defined(OS_CHROMEOS)
  // On CrOS the avatar toolbar button should only show as badging for Incognito
  // and Guest sessions. It should not be instantiated for regular profiles and
  // it should not be enabled as there's no profile switcher to trigger / show.
  DCHECK(IsIncognito() || profile_->IsGuestSession());
  SetEnabled(false);
#else
  // The profile switcher is only available outside incognito.
  SetEnabled(!IsIncognito());
#endif  // !defined(OS_CHROMEOS)

  // Set initial tooltip. UpdateIcon() needs to be called from the outside as
  // GetThemeProvider() is not available until the button is added to
  // ToolbarView's hierarchy.
  UpdateTooltipText();
}

AvatarToolbarButton::~AvatarToolbarButton() = default;

void AvatarToolbarButton::UpdateIcon() {
  SetImage(views::Button::STATE_NORMAL, GetAvatarIcon());
}

void AvatarToolbarButton::UpdateTooltipText() {
  SetTooltipText(GetAvatarTooltipText());
}

void AvatarToolbarButton::OnAvatarErrorChanged() {
  UpdateIcon();
  UpdateTooltipText();
}

void AvatarToolbarButton::OnProfileAdded(const base::FilePath& profile_path) {
  // Adding any profile changes the profile count, we might go from showing a
  // generic avatar button to profile pictures here. Update icon accordingly.
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  // Removing a profile changes the profile count, we might go from showing
  // per-profile icons back to a generic avatar icon. Update icon accordingly.
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  UpdateTooltipText();
}

void AvatarToolbarButton::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  UpdateIcon();
}

void AvatarToolbarButton::OnAccountImageUpdated(const std::string& account_id,
                                                const gfx::Image& image) {
  UpdateIcon();
}

bool AvatarToolbarButton::IsIncognito() const {
  return profile_->IsOffTheRecord() && !profile_->IsGuestSession();
}

bool AvatarToolbarButton::ShouldShowGenericIcon() const {
  // This function should only be used for regular profiles. Guest and Incognito
  // sessions should be handled separately and never call this function.
  DCHECK(!profile_->IsGuestSession());
  DCHECK(!profile_->IsOffTheRecord());
#if !defined(OS_CHROMEOS)
  if (!signin_ui_util::GetAccountsForDicePromos(profile_).empty())
    return false;
#endif  // !defined(OS_CHROMEOS)
  return g_browser_process->profile_manager()
                 ->GetProfileAttributesStorage()
                 .GetNumberOfProfiles() == 1 &&
         !SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated();
}

base::string16 AvatarToolbarButton::GetAvatarTooltipText() {
  if (IsIncognito())
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);

  if (profile_->IsGuestSession())
    return l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);

  if (ShouldShowGenericIcon())
    return l10n_util::GetStringUTF16(IDS_GENERIC_USER_AVATAR_LABEL);

  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(profile_->GetPath());
  switch (GetSyncState()) {
    case SyncState::kNormal:
      return profile_name;
    case SyncState::kPaused:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED,
                                        profile_name);
    case SyncState::kError:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR,
                                        profile_name);
  }

  NOTREACHED();
  return base::string16();
}

gfx::ImageSkia AvatarToolbarButton::GetAvatarIcon() {
  const int icon_size =
      ui::MaterialDesignController::IsTouchOptimizedUiEnabled() ? 24 : 20;

  const SkColor icon_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);

  if (IsIncognito())
    return gfx::CreateVectorIcon(kIncognitoIcon, icon_size, icon_color);

  if (profile_->IsGuestSession())
    return gfx::CreateVectorIcon(kUserMenuGuestIcon, icon_size, icon_color);

  gfx::Image avatar_icon;
  if (!ShouldShowGenericIcon()) {
    switch (GetSyncState()) {
      case SyncState::kNormal:
        avatar_icon = GetIconImageFromProfile();
        break;
      case SyncState::kPaused:
        return gfx::CreateVectorIcon(kSyncPausedIcon, icon_size,
                                     gfx::kGoogleBlue500);
      case SyncState::kError:
        return gfx::CreateVectorIcon(kSyncProblemIcon, icon_size,
                                     gfx::kGoogleRed700);
    }
  }

  if (!avatar_icon.IsEmpty()) {
    return profiles::GetSizedAvatarIcon(avatar_icon, true, icon_size, icon_size,
                                        profiles::SHAPE_CIRCLE)
        .AsImageSkia();
  }

  return gfx::CreateVectorIcon(kUserAccountAvatarIcon, icon_size, icon_color);
}

gfx::Image AvatarToolbarButton::GetIconImageFromProfile() const {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    // This can happen if the user deletes the current profile.
    return gfx::Image();
  }

  // If there is a GAIA image available, try to use that.
  if (entry->IsUsingGAIAPicture()) {
    // TODO(chengx): The GetGAIAPicture API call will trigger an async image
    // load from disk if it has not been loaded. This is non-obvious and
    // dependency should be avoided. We should come with a better idea to handle
    // this.
    const gfx::Image* gaia_image = entry->GetGAIAPicture();

    if (gaia_image)
      return *gaia_image;
    return gfx::Image();
  }

#if !defined(OS_CHROMEOS)
  // If the user isn't signed in and the profile icon wasn't changed explicitly,
  // try to use the first account icon of the sync promo.
  if (!SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated() &&
      entry->GetAvatarIconIndex() == 0) {
    std::vector<AccountInfo> promo_accounts =
        signin_ui_util::GetAccountsForDicePromos(profile_);
    if (!promo_accounts.empty()) {
      return AccountTrackerServiceFactory::GetForProfile(profile_)
          ->GetAccountImage(promo_accounts[0].account_id);
    }
  }
#endif  // !defined(OS_CHROMEOS)

  return entry->GetAvatarIcon();
}

AvatarToolbarButton::SyncState AvatarToolbarButton::GetSyncState() {
#if !defined(OS_CHROMEOS)
  if (profile_->IsSyncAllowed() && error_controller_.HasAvatarError()) {
    // When DICE is enabled and the error is an auth error, the sync-paused
    // icon is shown.
    int unused;
    const bool should_show_sync_paused_ui =
        AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
        sync_ui_util::GetMessagesForAvatarSyncError(
            profile_, *SigninManagerFactory::GetForProfile(profile_), &unused,
            &unused) == sync_ui_util::AUTH_ERROR;
    return should_show_sync_paused_ui ? SyncState::kPaused : SyncState::kError;
  }
  return SyncState::kNormal;
#else
  NOTREACHED();
  return SyncState::kError;
#endif  // !defined(OS_CHROMEOS)
}
