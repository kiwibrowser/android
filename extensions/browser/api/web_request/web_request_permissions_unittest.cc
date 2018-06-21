// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

TEST(ExtensionWebRequestPermissions, IsSensitiveRequest) {
  ExtensionsAPIClient api_client;
  struct TestCase {
    const char* url;
    bool is_sensitive_if_request_from_common_renderer;
    bool is_sensitive_if_request_from_browser_or_webui_renderer;
  } cases[] = {
      {"https://www.google.com", false, false},
      {"http://www.example.com", false, false},
      {"https://www.example.com", false, false},
      {"https://clients.google.com", false, true},
      {"https://clients4.google.com", false, true},
      {"https://clients9999.google.com", false, true},
      {"https://clients9999..google.com", false, false},
      {"https://clients9999.example.google.com", false, false},
      {"https://clients.google.com.", false, true},
      {"https://.clients.google.com.", false, true},
      {"http://google.example.com", false, false},
      {"http://www.example.com", false, false},
      {"https://www.example.com", false, false},
      {"https://clients.google.com", false, true},
      {"https://sb-ssl.google.com", true, true},
      {"https://sb-ssl.random.google.com", false, false},
      {"https://safebrowsing.googleapis.com", true, true},
      {"blob:https://safebrowsing.googleapis.com/"
       "fc3f440b-78ed-469f-8af8-7a1717ff39ae",
       true, true},
      {"filesystem:https://safebrowsing.googleapis.com/path", true, true},
      {"https://safebrowsing.googleapis.com.", true, true},
      {"https://safebrowsing.googleapis.com/v4", true, true},
      {"https://safebrowsing.googleapis.com:80/v4", true, true},
      {"https://safebrowsing.googleapis.com./v4", true, true},
      {"https://safebrowsing.googleapis.com/v5", true, true},
      {"https://safebrowsing.google.com/safebrowsing", true, true},
      {"https://safebrowsing.google.com/safebrowsing/anything", true, true},
      {"https://safebrowsing.google.com", false, false},
      {"https://chrome.google.com", false, false},
      {"https://chrome.google.com/webstore", true, true},
      {"https://chrome.google.com./webstore", true, true},
      {"blob:https://chrome.google.com/fc3f440b-78ed-469f-8af8-7a1717ff39ae",
       false, false},
      {"https://chrome.google.com:80/webstore", true, true},
      {"https://chrome.google.com/webstore?query", true, true},
  };
  for (const TestCase& test : cases) {
    WebRequestInfo request;
    request.url = GURL(test.url);
    EXPECT_TRUE(request.url.is_valid()) << test.url;

    request.initiator = url::Origin::Create(request.url);
    EXPECT_EQ(test.is_sensitive_if_request_from_common_renderer,
              IsSensitiveRequest(request, false /* is_request_from_browser */,
                                 false /* is_request_from_web_ui_renderer */))
        << test.url;

    const bool supported_in_webui_renderers =
        !request.url.SchemeIsHTTPOrHTTPS();
    request.initiator = base::nullopt;
    EXPECT_EQ(test.is_sensitive_if_request_from_browser_or_webui_renderer,
              IsSensitiveRequest(request, true /* is_request_from_browser */,
                                 supported_in_webui_renderers))
        << test.url;
  }
}

TEST(ExtensionWebRequestPermissions,
     CanExtensionAccessURLWithWithheldPermissions) {
  // The InfoMap requires methods to be called on the IO thread. Fake it.
  content::TestBrowserThreadBundle thread_bundle(
      content::TestBrowserThreadBundle::IO_MAINLOOP);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").AddPermission("<all_urls>").Build();
  URLPatternSet all_urls(
      {URLPattern(Extension::kValidHostPermissionSchemes, "<all_urls>")});
  // Simulate withholding the <all_urls> permission.
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(),  // active permissions.
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(), all_urls,
          URLPatternSet()) /* withheld permissions */);

  scoped_refptr<InfoMap> info_map = base::MakeRefCounted<InfoMap>();
  info_map->AddExtension(extension.get(), base::Time(), false, false);

  auto get_access = [extension, info_map](
                        const GURL& url,
                        base::Optional<url::Origin> initiator) {
    constexpr int kTabId = 42;
    constexpr WebRequestPermissions::HostPermissionsCheck kPermissionsCheck =
        WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL;
    return WebRequestPermissions::CanExtensionAccessURL(
        info_map.get(), extension->id(), url, kTabId,
        false /* crosses incognito */, kPermissionsCheck, initiator);
  };

  const GURL example_com("https://example.com");
  const GURL chromium_org("https://chromium.org");
  const url::Origin example_com_origin(url::Origin::Create(example_com));
  const url::Origin chromium_org_origin(url::Origin::Create(chromium_org));

  // With all permissions withheld, the result of any request should be
  // kWithheld.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, example_com_origin));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, chromium_org_origin));

  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(chromium_org, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(chromium_org, chromium_org_origin));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(chromium_org, example_com_origin));

  // Grant access to chromium.org.
  URLPatternSet chromium_org_patterns({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://chromium.org/*")});
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(APIPermissionSet(),
                                      ManifestPermissionSet(),
                                      chromium_org_patterns, URLPatternSet()),
      std::make_unique<PermissionSet>(APIPermissionSet(),
                                      ManifestPermissionSet(), all_urls,
                                      URLPatternSet()));

  // example.com isn't granted, so without an initiator or with an initiator
  // that the extension doesn't have access to, access is withheld.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, example_com_origin));

  // However, if a request is made to example.com from an initiator that the
  // extension has access to, access is allowed. This is functionally necessary
  // for any extension with webRequest to work with the runtime host permissions
  // feature. See https://crbug.com/851722.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(example_com, chromium_org_origin));

  // With access to the requested origin, access is always allowed, independent
  // of initiator.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(chromium_org, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(chromium_org, chromium_org_origin));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(chromium_org, example_com_origin));
}

}  // namespace extensions
