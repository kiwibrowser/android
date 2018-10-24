// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_writer.h"

namespace resource_coordinator {

LocalSiteCharacteristicsDataWriter::~LocalSiteCharacteristicsDataWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_loaded_)
    NotifySiteUnloaded();
}

void LocalSiteCharacteristicsDataWriter::NotifySiteLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!is_loaded_);
  is_loaded_ = true;
  impl_->NotifySiteLoaded();

  if (tab_visibility_ == TabVisibility::kBackground)
    impl_->NotifyLoadedSiteBackgrounded();
}

void LocalSiteCharacteristicsDataWriter::NotifySiteUnloaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_loaded_);

  is_loaded_ = false;

  impl_->NotifySiteUnloaded(tab_visibility_);
}

void LocalSiteCharacteristicsDataWriter::NotifySiteVisibilityChanged(
    TabVisibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore this if we receive the same event multiple times.
  if (tab_visibility_ == visibility)
    return;

  tab_visibility_ = visibility;

  if (is_loaded_) {
    if (visibility == TabVisibility::kBackground) {
      impl_->NotifyLoadedSiteBackgrounded();
    } else {
      impl_->NotifyLoadedSiteForegrounded();
    }
  }
}

void LocalSiteCharacteristicsDataWriter::NotifyUpdatesFaviconInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(TabVisibility::kBackground, tab_visibility_);
  impl_->NotifyUpdatesFaviconInBackground();
}

void LocalSiteCharacteristicsDataWriter::NotifyUpdatesTitleInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(TabVisibility::kBackground, tab_visibility_);
  impl_->NotifyUpdatesTitleInBackground();
}

void LocalSiteCharacteristicsDataWriter::NotifyUsesAudioInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(TabVisibility::kBackground, tab_visibility_);
  impl_->NotifyUsesAudioInBackground();
}

void LocalSiteCharacteristicsDataWriter::NotifyUsesNotificationsInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(TabVisibility::kBackground, tab_visibility_);
  impl_->NotifyUsesNotificationsInBackground();
}

LocalSiteCharacteristicsDataWriter::LocalSiteCharacteristicsDataWriter(
    scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl,
    TabVisibility tab_visibility)
    : impl_(std::move(impl)),
      tab_visibility_(tab_visibility),
      is_loaded_(false) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace resource_coordinator
