// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojo/clipboard_host.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace ui {

ClipboardHost::ClipboardHost()
    : clipboard_(Clipboard::GetForCurrentThread()),
      clipboard_writer_(new ScopedClipboardWriter(CLIPBOARD_TYPE_COPY_PASTE)) {}

ClipboardHost::~ClipboardHost() {
  clipboard_writer_->Reset();
}

void ClipboardHost::AddBinding(mojom::ClipboardHostRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ClipboardHost::GetSequenceNumber(ClipboardType type,
                                      GetSequenceNumberCallback callback) {
  std::move(callback).Run(clipboard_->GetSequenceNumber(type));
}

void ClipboardHost::IsFormatAvailable(const std::string& format,
                                      ClipboardType type,
                                      IsFormatAvailableCallback callback) {
  auto format_type = Clipboard::FormatType::Deserialize(format);
  bool result = clipboard_->IsFormatAvailable(format_type, type);
  std::move(callback).Run(result);
}

void ClipboardHost::Clear(ClipboardType type) {
  clipboard_->Clear(type);
}

void ClipboardHost::ReadAvailableTypes(ClipboardType type,
                                       ReadAvailableTypesCallback callback) {
  std::vector<base::string16> types;
  bool contains_filenames;
  clipboard_->ReadAvailableTypes(type, &types, &contains_filenames);
  std::move(callback).Run(types, contains_filenames);
}

void ClipboardHost::ReadText(ClipboardType type, ReadTextCallback callback) {
  base::string16 result;
  if (clipboard_->IsFormatAvailable(Clipboard::GetPlainTextWFormatType(),
                                    type)) {
    clipboard_->ReadText(type, &result);
  } else if (clipboard_->IsFormatAvailable(Clipboard::GetPlainTextFormatType(),
                                           type)) {
    std::string ascii;
    clipboard_->ReadAsciiText(type, &ascii);
    result = base::ASCIIToUTF16(ascii);
  }
  std::move(callback).Run(result);
}

void ClipboardHost::ReadAsciiText(ClipboardType type,
                                  ReadAsciiTextCallback callback) {
  std::string ascii_text;
  clipboard_->ReadAsciiText(type, &ascii_text);
  std::move(callback).Run(std::move(ascii_text));
}

void ClipboardHost::ReadHTML(ClipboardType type, ReadHTMLCallback callback) {
  base::string16 markup;
  std::string src_url_str;
  uint32_t fragment_start = 0;
  uint32_t fragment_end = 0;
  clipboard_->ReadHTML(type, &markup, &src_url_str, &fragment_start,
                       &fragment_end);
  std::move(callback).Run(std::move(markup), src_url_str, fragment_start,
                          fragment_end);
}

void ClipboardHost::ReadRTF(ClipboardType type, ReadRTFCallback callback) {
  std::string result;
  clipboard_->ReadRTF(type, &result);
  std::move(callback).Run(result);
}

void ClipboardHost::ReadImage(ClipboardType type, ReadImageCallback callback) {
  SkBitmap result = clipboard_->ReadImage(type);
  std::move(callback).Run(result);
}

void ClipboardHost::ReadCustomData(ClipboardType clipboard_type,
                                   const base::string16& type,
                                   ReadCustomDataCallback callback) {
  base::string16 result;
  clipboard_->ReadCustomData(clipboard_type, type, &result);
  std::move(callback).Run(result);
}

void ClipboardHost::ReadBookmark(ReadBookmarkCallback callback) {
  base::string16 title;
  std::string url;
  clipboard_->ReadBookmark(&title, &url);
  std::move(callback).Run(std::move(title), url);
}

void ClipboardHost::ReadData(const std::string& format,
                             ReadDataCallback callback) {
  std::string result;
  clipboard_->ReadData(Clipboard::FormatType::Deserialize(format), &result);
  std::move(callback).Run(std::move(result));
}

void ClipboardHost::GetLastModifiedTime(GetLastModifiedTimeCallback callback) {
  std::move(callback).Run(clipboard_->GetLastModifiedTime());
}

void ClipboardHost::ClearLastModifiedTime() {
  clipboard_->ClearLastModifiedTime();
}

void ClipboardHost::WriteText(const base::string16& text) {
  clipboard_writer_->WriteText(text);
}

void ClipboardHost::WriteHTML(const base::string16& markup,
                              const std::string& url) {
  clipboard_writer_->WriteHTML(markup, url);
}

void ClipboardHost::WriteRTF(const std::string& rtf) {
  clipboard_writer_->WriteRTF(rtf);
}

void ClipboardHost::WriteBookmark(const std::string& url,
                                  const base::string16& title) {
  clipboard_writer_->WriteBookmark(title, url);
}

void ClipboardHost::WriteWebSmartPaste() {
  clipboard_writer_->WriteWebSmartPaste();
}

void ClipboardHost::WriteBitmap(const SkBitmap& bitmap) {
  clipboard_writer_->WriteImage(bitmap);
}

void ClipboardHost::WriteData(const std::string& type,
                              const std::string& data) {
  clipboard_writer_->WriteData(type, data);
}

void ClipboardHost::CommitWrite(ClipboardType type) {
  // Set the target type before the writer commits its data on destruction.
  clipboard_writer_->set_type(type);
  clipboard_writer_ = std::make_unique<ScopedClipboardWriter>(type);
}

}  // namespace ui
