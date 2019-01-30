// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojo/clipboard_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"

namespace ui {

ClipboardClient::ClipboardClient(mojom::ClipboardHostPtr clipboard)
    : clipboard_(std::move(clipboard)) {}

ClipboardClient::~ClipboardClient() {}

void ClipboardClient::OnPreShutdown() {}

uint64_t ClipboardClient::GetSequenceNumber(ClipboardType type) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  uint64_t sequence_number = 0;
  clipboard_->GetSequenceNumber(type, &sequence_number);
  return sequence_number;
}

bool ClipboardClient::IsFormatAvailable(const FormatType& format,
                                        ClipboardType type) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  bool result = false;
  clipboard_->IsFormatAvailable(format.Serialize(), type, &result);
  return result;
}

void ClipboardClient::Clear(ClipboardType type) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->Clear(type);
}

void ClipboardClient::ReadAvailableTypes(ClipboardType type,
                                         std::vector<base::string16>* types,
                                         bool* contains_filenames) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadAvailableTypes(type, types, contains_filenames);
}

void ClipboardClient::ReadText(ClipboardType type,
                               base::string16* result) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadText(type, result);
}

void ClipboardClient::ReadAsciiText(ClipboardType type,
                                    std::string* result) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadAsciiText(type, result);
}

void ClipboardClient::ReadHTML(ClipboardType type,
                               base::string16* markup,
                               std::string* src_url,
                               uint32_t* fragment_start,
                               uint32_t* fragment_end) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadHTML(type, markup, src_url, fragment_start, fragment_end);
}

void ClipboardClient::ReadRTF(ClipboardType type, std::string* result) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadRTF(type, result);
}

SkBitmap ClipboardClient::ReadImage(ClipboardType type) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  SkBitmap result;
  clipboard_->ReadImage(type, &result);
  return result;
}

void ClipboardClient::ReadCustomData(ClipboardType clipboard_type,
                                     const base::string16& type,
                                     base::string16* result) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadCustomData(clipboard_type, type, result);
}

void ClipboardClient::ReadBookmark(base::string16* title,
                                   std::string* url) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadBookmark(title, url);
}

void ClipboardClient::ReadData(const FormatType& format,
                               std::string* result) const {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->ReadData(format.Serialize(), result);
}

void ClipboardClient::WriteObjects(ClipboardType type,
                                   const ObjectMap& objects) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  for (const auto& object : objects)
    DispatchObject(static_cast<ObjectType>(object.first), object.second);
  clipboard_->CommitWrite(type);
}

void ClipboardClient::WriteText(const char* text_data, size_t text_len) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  base::string16 text;
  base::UTF8ToUTF16(text_data, text_len, &text);
  clipboard_->WriteText(text);
}

void ClipboardClient::WriteHTML(const char* markup_data,
                                size_t markup_len,
                                const char* url_data,
                                size_t url_len) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  base::string16 markup;
  base::UTF8ToUTF16(markup_data, markup_len, &markup);
  clipboard_->WriteHTML(markup, std::string(url_data, url_len));
}

void ClipboardClient::WriteRTF(const char* rtf_data, size_t data_len) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->WriteRTF(std::string(rtf_data, data_len));
}

void ClipboardClient::WriteBookmark(const char* title_data,
                                    size_t title_len,
                                    const char* url_data,
                                    size_t url_len) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  base::string16 title;
  base::UTF8ToUTF16(title_data, title_len, &title);
  clipboard_->WriteBookmark(std::string(url_data, url_len), title);
}

void ClipboardClient::WriteWebSmartPaste() {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->WriteWebSmartPaste();
}

void ClipboardClient::WriteBitmap(const SkBitmap& bitmap) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->WriteBitmap(bitmap);
}

void ClipboardClient::WriteData(const FormatType& format,
                                const char* data_data,
                                size_t data_len) {
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  clipboard_->WriteData(format.Serialize(), std::string(data_data, data_len));
}

}  // namespace ui
