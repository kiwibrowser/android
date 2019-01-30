// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/chromeos/smb_client/discovery/mdns_host_locator.h"
#include "chrome/browser/chromeos/smb_client/smb_constants.h"

namespace chromeos {
namespace smb_client {

SmbShareFinder::SmbShareFinder(SmbProviderClient* client) : client_(client) {}
SmbShareFinder::~SmbShareFinder() = default;

void SmbShareFinder::GatherSharesInNetwork(GatherSharesResponse callback) {
  scanner_.FindHostsInNetwork(base::BindOnce(&SmbShareFinder::OnHostsFound,
                                             AsWeakPtr(), std::move(callback)));
}

void SmbShareFinder::RegisterHostLocator(std::unique_ptr<HostLocator> locator) {
  scanner_.RegisterHostLocator(std::move(locator));
}

std::string SmbShareFinder::GetResolvedUrl(const SmbUrl& url) const {
  DCHECK(url.IsValid());

  const std::string ip_address = scanner_.ResolveHost(url.GetHost());
  if (ip_address.empty()) {
    return url.ToString();
  }

  return url.ReplaceHost(ip_address);
}

void SmbShareFinder::OnHostsFound(GatherSharesResponse callback,
                                  bool success,
                                  const HostMap& hosts) {
  if (!success) {
    LOG(ERROR) << "SmbShareFinder failed to find hosts";
    callback.Run(std::vector<SmbUrl>());
    return;
  }

  if (hosts.empty()) {
    callback.Run(std::vector<SmbUrl>());
    return;
  }

  for (const auto& host : hosts) {
    const std::string& host_name = host.first;
    const std::string& resolved_address = host.second;
    const base::FilePath server_url(kSmbSchemePrefix + resolved_address);

    client_->GetShares(
        server_url, base::BindOnce(&SmbShareFinder::OnSharesFound, AsWeakPtr(),
                                   host_name, callback));
  }
}

void SmbShareFinder::OnSharesFound(
    const std::string& host_name,
    GatherSharesResponse callback,
    smbprovider::ErrorType error,
    const smbprovider::DirectoryEntryListProto& entries) {
  if (error != smbprovider::ErrorType::ERROR_OK) {
    LOG(ERROR) << "Error finding shares: " << error;
    callback.Run(std::vector<SmbUrl>());
    return;
  }

  std::vector<SmbUrl> shares;
  for (const smbprovider::DirectoryEntryProto& entry : entries.entries()) {
    SmbUrl url(kSmbSchemePrefix + host_name + "/" + entry.name());
    if (url.IsValid()) {
      shares.push_back(std::move(url));
    } else {
      LOG(WARNING) << "URL found is not valid";
    }
  }

  callback.Run(shares);
}

}  // namespace smb_client
}  // namespace chromeos
