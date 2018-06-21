// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_network_delegate.h"

#include "base/test/scoped_task_environment.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

const GURL kURL("http://foo.com");
const GURL kOtherURL("http://other.com");

class NetworkServiceNetworkDelegateTest : public testing::Test {
 public:
  NetworkServiceNetworkDelegateTest()
      : network_service_(NetworkService::CreateForTesting()) {
    mojom::NetworkContextPtr network_context_ptr;
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(), mojo::MakeRequest(&network_context_ptr),
        mojom::NetworkContextParams::New());
  }

  void SetContentSetting(const GURL& primary_pattern,
                         const GURL& secondary_pattern,
                         ContentSetting setting) {
    network_context_->cookie_manager()->SetContentSettings(
        {ContentSettingPatternSource(
            ContentSettingsPattern::FromURL(primary_pattern),
            ContentSettingsPattern::FromURL(secondary_pattern),
            base::Value(setting), std::string(), false)});
  }

  void SetBlockThirdParty(bool block) {
    network_context_->cookie_manager()->BlockThirdPartyCookies(block);
  }

  NetworkContext* network_context() const { return network_context_.get(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
};

TEST_F(NetworkServiceNetworkDelegateTest, PrivacyModeDisabledByDefault) {
  NetworkServiceNetworkDelegate delegate(network_context());

  EXPECT_FALSE(delegate.CanEnablePrivacyMode(kURL, kOtherURL));
}

TEST_F(NetworkServiceNetworkDelegateTest, PrivacyModeEnabledIfCookiesBlocked) {
  NetworkServiceNetworkDelegate delegate(network_context());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(delegate.CanEnablePrivacyMode(kURL, kOtherURL));
}

TEST_F(NetworkServiceNetworkDelegateTest, PrivacyModeDisabledIfCookiesAllowed) {
  NetworkServiceNetworkDelegate delegate(network_context());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(delegate.CanEnablePrivacyMode(kURL, kOtherURL));
}

TEST_F(NetworkServiceNetworkDelegateTest,
       PrivacyModeDisabledIfCookiesSettingForOtherURL) {
  NetworkServiceNetworkDelegate delegate(network_context());

  // URLs are switched so setting should not apply.
  SetContentSetting(kOtherURL, kURL, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(delegate.CanEnablePrivacyMode(kURL, kOtherURL));
}

TEST_F(NetworkServiceNetworkDelegateTest,
       PrivacyModeEnabledIfThirdPartyCookiesBlocked) {
  NetworkServiceNetworkDelegate delegate(network_context());

  SetBlockThirdParty(true);
  EXPECT_TRUE(delegate.CanEnablePrivacyMode(kURL, kOtherURL));
  EXPECT_FALSE(delegate.CanEnablePrivacyMode(kURL, kURL));

  SetBlockThirdParty(false);
  EXPECT_FALSE(delegate.CanEnablePrivacyMode(kURL, kOtherURL));
  EXPECT_FALSE(delegate.CanEnablePrivacyMode(kURL, kURL));
}

}  // namespace
}  // namespace network
