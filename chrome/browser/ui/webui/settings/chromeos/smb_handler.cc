// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/smb_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {
namespace settings {

namespace {

smb_client::SmbService* GetSmbService(Profile* profile) {
  smb_client::SmbService* const service = smb_client::SmbService::Get(profile);
  DCHECK(service);

  return service;
}

}  // namespace

SmbHandler::SmbHandler(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

SmbHandler::~SmbHandler() = default;

void SmbHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "smbMount",
      base::BindRepeating(&SmbHandler::HandleSmbMount, base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startDiscovery", base::BindRepeating(&SmbHandler::HandleStartDiscovery,
                                            base::Unretained(this)));
}

void SmbHandler::HandleSmbMount(const base::ListValue* args) {
  CHECK_EQ(4U, args->GetSize());
  std::string mount_url;
  std::string mount_name;
  std::string username;
  std::string password;
  CHECK(args->GetString(0, &mount_url));
  CHECK(args->GetString(1, &mount_name));
  CHECK(args->GetString(2, &username));
  CHECK(args->GetString(3, &password));

  smb_client::SmbService* const service = GetSmbService(profile_);

  chromeos::file_system_provider::MountOptions mo;
  mo.display_name = mount_name.empty() ? mount_url : mount_name;
  mo.writable = true;

  service->Mount(mo, base::FilePath(mount_url), username, password,
                 base::BindOnce(&SmbHandler::HandleSmbMountResponse,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SmbHandler::HandleSmbMountResponse(SmbMountResult result) {
  AllowJavascript();
  FireWebUIListener("on-add-smb-share", base::Value(static_cast<int>(result)));
}

void SmbHandler::HandleStartDiscovery(const base::ListValue* args) {
  smb_client::SmbService* const service = GetSmbService(profile_);

  service->GatherSharesInNetwork(base::BindRepeating(
      &SmbHandler::HandleGatherSharesResponse, weak_ptr_factory_.GetWeakPtr()));
}

void SmbHandler::HandleGatherSharesResponse(
    const std::vector<smb_client::SmbUrl>& shares_gathered) {
  // TODO(zentaro): Pass the shares discovered back to the UI.
  // https://crbug.com/852199.
}

}  // namespace settings
}  // namespace chromeos
