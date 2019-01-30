// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/smb_client/discovery/in_memory_host_locator.h"
#include "chrome/browser/chromeos/smb_client/smb_constants.h"
#include "chrome/browser/chromeos/smb_client/smb_url.h"
#include "chromeos/dbus/fake_smb_provider_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {

namespace {

constexpr char kDefaultHost[] = "host";
constexpr char kDefaultAddress[] = "1.2.3.4";
constexpr char kDefaultUrl[] = "smb://host/";
constexpr char kDefaultResolvedUrl[] = "smb://1.2.3.4";

}  // namespace

class SmbShareFinderTest : public testing::Test {
 public:
  SmbShareFinderTest() {
    auto host_locator = std::make_unique<InMemoryHostLocator>();
    host_locator_ = host_locator.get();

    fake_client_ = std::make_unique<FakeSmbProviderClient>();
    share_finder_ = std::make_unique<SmbShareFinder>(fake_client_.get());

    share_finder_->RegisterHostLocator(std::move(host_locator));
  }

  ~SmbShareFinderTest() override = default;

  void TearDown() override { fake_client_->ClearShares(); }

  // Adds host with |hostname| and |address| as the resolved url.
  void AddHost(const std::string& hostname, const std::string& address) {
    host_locator_->AddHost(hostname, address);
  }

  // Adds the default host with the default address.
  void AddDefaultHost() { AddHost(kDefaultHost, kDefaultAddress); }

  // Adds |share| to the default host.
  void AddShareToDefaultHost(const std::string& share) {
    AddShare(kDefaultResolvedUrl, kDefaultUrl, share);
  }

  // Adds |share| a host. |resolved_url| will be in the format of
  // "smb://1.2.3.4" and |server_url| will be in the format of "smb://host/".
  void AddShare(const std::string& resolved_url,
                const std::string& server_url,
                const std::string& share) {
    fake_client_->AddToShares(resolved_url, share);
    expected_shares_.insert(server_url + share);
  }

  // Helper function when expecting shares to be found in the network.
  void ExpectSharesFound() {
    share_finder_->GatherSharesInNetwork(base::BindRepeating(
        &SmbShareFinderTest::SharesFoundCallback, base::Unretained(this)));
  }

  // Helper function when expecting no shares to be found in the network.
  void ExpectNoSharesFound() {
    share_finder_->GatherSharesInNetwork(base::BindRepeating(
        &SmbShareFinderTest::EmptySharesCallback, base::Unretained(this)));
  }

  // Helper function that expects expected_shares_ to be empty.
  void ExpectAllSharesHaveBeenFound() { EXPECT_TRUE(expected_shares_.empty()); }

  // Helper function that expects |url| to resolve to |expected|.
  void ExpectResolvedHost(const SmbUrl& url, const std::string& expected) {
    EXPECT_EQ(expected, share_finder_->GetResolvedUrl(url));
  }

 private:
  // Removes shares discovered from expected_shares_.
  void SharesFoundCallback(const std::vector<SmbUrl>& shares_found) {
    EXPECT_GE(shares_found.size(), 0u);

    for (const SmbUrl& url : shares_found) {
      EXPECT_EQ(1u, expected_shares_.erase(url.ToString()));
    }
  }

  void EmptySharesCallback(const std::vector<SmbUrl>& shares_found) {
    EXPECT_EQ(0u, shares_found.size());
  }

  // Keeps track of expected shares across multiple hosts.
  std::set<std::string> expected_shares_;

  InMemoryHostLocator* host_locator_;
  std::unique_ptr<FakeSmbProviderClient> fake_client_;
  std::unique_ptr<SmbShareFinder> share_finder_;

  DISALLOW_COPY_AND_ASSIGN(SmbShareFinderTest);
};

TEST_F(SmbShareFinderTest, NoSharesFoundWithNoHosts) {
  ExpectNoSharesFound();
}

TEST_F(SmbShareFinderTest, NoSharesFoundWithEmptyHost) {
  AddDefaultHost();
  ExpectNoSharesFound();
}

TEST_F(SmbShareFinderTest, NoSharesFoundWithMultipleEmptyHosts) {
  AddDefaultHost();
  AddHost("host2", "4.5.6.7");
  ExpectNoSharesFound();
}

TEST_F(SmbShareFinderTest, SharesFoundWithSingleHost) {
  AddDefaultHost();
  AddShareToDefaultHost("share1");
  AddShareToDefaultHost("share2");

  ExpectSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, SharesFoundWithMultipleHosts) {
  AddDefaultHost();
  AddShareToDefaultHost("share1");

  const std::string host2 = "host2";
  const std::string address2 = "4.5.6.7";
  const std::string resolved_server_url2 = kSmbSchemePrefix + address2;
  const std::string server_url2 = kSmbSchemePrefix + host2 + "/";
  const std::string share2 = "share2";
  AddHost(host2, address2);
  AddShare(resolved_server_url2, server_url2, share2);

  ExpectSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, SharesFoundOnOneHostWithMultipleHosts) {
  AddDefaultHost();
  AddShareToDefaultHost("share1");

  AddHost("host2", "4.5.6.7");

  ExpectSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, ResolvesHostToOriginalUrlIfNoHostFound) {
  const std::string url = std::string(kDefaultUrl) + "share";
  SmbUrl smb_url(url);

  // Trigger the NetworkScanner to scan the network with its HostLocators.
  ExpectSharesFound();

  ExpectResolvedHost(smb_url, url);
}

TEST_F(SmbShareFinderTest, ResolvesHost) {
  AddDefaultHost();

  // Trigger the NetworkScanner to scan the network with its HostLocators.
  ExpectSharesFound();

  SmbUrl url(std::string(kDefaultUrl) + "share");
  ExpectResolvedHost(url, std::string(kDefaultResolvedUrl) + "/share");
}

TEST_F(SmbShareFinderTest, ResolvesHostWithMultipleHosts) {
  AddDefaultHost();
  AddHost("host2", "4.5.6.7");

  // Trigger the NetworkScanner to scan the network with its HostLocators.
  ExpectSharesFound();

  SmbUrl url("smb://host2/share");
  ExpectResolvedHost(url, "smb://4.5.6.7/share");
}

}  // namespace smb_client
}  // namespace chromeos
