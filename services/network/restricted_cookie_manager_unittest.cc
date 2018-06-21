// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "mojo/edk/embedder/embedder.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

// Synchronous proxies to a wrapped RestrictedCookieManager's methods.
class RestrictedCookieManagerSync {
 public:
  // Caller must guarantee that |*cookie_service| outlives the
  // SynchronousCookieManager.
  explicit RestrictedCookieManagerSync(
      mojom::RestrictedCookieManager* cookie_service)
      : cookie_service_(cookie_service) {}
  ~RestrictedCookieManagerSync() {}

  std::vector<net::CanonicalCookie> GetAllForUrl(
      const GURL& url,
      const GURL& site_for_cookies,
      mojom::CookieManagerGetOptionsPtr options) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> result;
    cookie_service_->GetAllForUrl(
        url, site_for_cookies, std::move(options),
        base::BindLambdaForTesting(
            [&run_loop,
             &result](const std::vector<net::CanonicalCookie>& backend_result) {
              result = backend_result;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const GURL& site_for_cookies) {
    base::RunLoop run_loop;
    bool result = false;
    cookie_service_->SetCanonicalCookie(
        cookie, url, site_for_cookies,
        base::BindLambdaForTesting([&run_loop, &result](bool backend_result) {
          result = backend_result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void AddChangeListener(const GURL& url,
                         const GURL& site_for_cookies,
                         mojom::CookieChangeListenerPtr listener) {
    base::RunLoop run_loop;
    cookie_service_->AddChangeListener(
        url, site_for_cookies, std::move(listener), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  mojom::RestrictedCookieManager* cookie_service_;

  DISALLOW_COPY_AND_ASSIGN(RestrictedCookieManagerSync);
};

class RestrictedCookieManagerTest : public testing::Test {
 public:
  RestrictedCookieManagerTest()
      : cookie_monster_(nullptr, nullptr),
        service_(std::make_unique<RestrictedCookieManager>(
            &cookie_monster_,
            url::Origin::Create(GURL("http://example.com")))),
        binding_(service_.get(), mojo::MakeRequest(&service_ptr_)) {
    sync_service_ =
        std::make_unique<RestrictedCookieManagerSync>(service_ptr_.get());
  }
  ~RestrictedCookieManagerTest() override {}

  void SetUp() override {
    mojo::edk::SetDefaultProcessErrorCallback(base::BindRepeating(
        &RestrictedCookieManagerTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    mojo::edk::SetDefaultProcessErrorCallback(
        mojo::edk::ProcessErrorCallback());
  }

  // Set a canonical cookie directly into the store.
  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool can_modify_httponly) {
    net::ResultSavingCookieCallback<bool> callback;
    cookie_monster_.SetCanonicalCookieAsync(
        std::make_unique<net::CanonicalCookie>(cookie), secure_source,
        can_modify_httponly,
        base::BindOnce(&net::ResultSavingCookieCallback<bool>::Run,
                       base::Unretained(&callback)));
    callback.WaitUntilDone();
    return callback.result();
  }

  // Simplified helper for SetCanonicalCookie.
  //
  // Creates a CanonicalCookie that is not secure, not http-only,
  // and not restricted to first parties. Crashes if the creation fails.
  void SetSessionCookie(const char* name,
                        const char* value,
                        const char* domain,
                        const char* path) {
    CHECK(SetCanonicalCookie(
        net::CanonicalCookie(name, value, domain, path, base::Time(),
                             base::Time(), base::Time(), /* secure = */ false,
                             /* httponly = */ false,
                             net::CookieSameSite::NO_RESTRICTION,
                             net::COOKIE_PRIORITY_DEFAULT),
        /* secure_source = */ true, /* can_modify_httponly = */ true));
  }

  void ExpectBadMessage() { expecting_bad_message_ = true; }

  bool received_bad_message() { return received_bad_message_; }

 protected:
  void OnBadMessage(const std::string& reason) {
    EXPECT_TRUE(expecting_bad_message_) << "Unexpected bad message: " << reason;
    received_bad_message_ = true;
  }

  base::MessageLoopForIO message_loop_;
  net::CookieMonster cookie_monster_;
  std::unique_ptr<RestrictedCookieManager> service_;
  mojom::RestrictedCookieManagerPtr service_ptr_;
  mojo::Binding<mojom::RestrictedCookieManager> binding_;
  std::unique_ptr<RestrictedCookieManagerSync> sync_service_;
  bool expecting_bad_message_ = false;
  bool received_bad_message_ = false;
};

namespace {

bool CompareCanonicalCookies(const net::CanonicalCookie& c1,
                             const net::CanonicalCookie& c2) {
  return c1.FullCompare(c2);
}

}  // anonymous namespace

TEST_F(RestrictedCookieManagerTest, GetAllForUrlBlankFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(2));
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("cookie-name", cookies[0].Name());
  EXPECT_EQ("cookie-value", cookies[0].Value());

  EXPECT_EQ("cookie-name-2", cookies[1].Name());
  EXPECT_EQ("cookie-value-2", cookies[1].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlEmptyFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(0));
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlEqualsMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(1));

  EXPECT_EQ("cookie-name", cookies[0].Name());
  EXPECT_EQ("cookie-value", cookies[0].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlStartsWithMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("cookie-name-2b", "cookie-value-2b", "example.com", "/");
  SetSessionCookie("cookie-name-3b", "cookie-value-3b", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name-2";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(2));
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("cookie-name-2", cookies[0].Name());
  EXPECT_EQ("cookie-value-2", cookies[0].Value());

  EXPECT_EQ("cookie-name-2b", cookies[1].Name());
  EXPECT_EQ("cookie-value-2b", cookies[1].Value());
}

TEST_F(RestrictedCookieManagerTest, GetAllForUrlFromWrongOrigin) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  ExpectBadMessage();
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://not-example.com/test/"), GURL("http://not-example.com"),
      std::move(options));
  EXPECT_TRUE(received_bad_message());

  ASSERT_THAT(cookies, testing::SizeIs(0));
}

TEST_F(RestrictedCookieManagerTest, SetCanonicalCookie) {
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      net::CanonicalCookie(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      GURL("http://example.com/test/"), GURL("http://example.com")));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("http://example.com/test/"), GURL("http://example.com"),
      std::move(options));

  ASSERT_THAT(cookies, testing::SizeIs(1));

  EXPECT_EQ("new-name", cookies[0].Name());
  EXPECT_EQ("new-value", cookies[0].Value());
}

TEST_F(RestrictedCookieManagerTest, SetCanonicalCookieFromWrongOrigin) {
  ExpectBadMessage();
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      net::CanonicalCookie(
          "new-name", "new-value", "not-example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      GURL("http://not-example.com/test/"), GURL("http://not-example.com")));
  ASSERT_TRUE(received_bad_message());
}

namespace {

// Stashes the cookie changes it receives, for testing.
class TestCookieChangeListener : public network::mojom::CookieChangeListener {
 public:
  // Records a cookie change received from RestrictedCookieManager.
  struct Change {
    Change(const net::CanonicalCookie& cookie,
           network::mojom::CookieChangeCause change_cause)
        : cookie(cookie), change_cause(change_cause) {}

    net::CanonicalCookie cookie;
    network::mojom::CookieChangeCause change_cause;
  };

  TestCookieChangeListener(network::mojom::CookieChangeListenerRequest request)
      : binding_(this, std::move(request)) {}
  ~TestCookieChangeListener() override = default;

  // Spin in a run loop until a change is received.
  void WaitForChange() {
    base::RunLoop loop;
    run_loop_ = &loop;
    loop.Run();
    run_loop_ = nullptr;
  }

  // Changes received by this listener.
  const std::vector<Change>& observed_changes() const {
    return observed_changes_;
  }

  // network::mojom::CookieChangeListener
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      network::mojom::CookieChangeCause change_cause) override {
    observed_changes_.emplace_back(cookie, change_cause);

    if (run_loop_)  // Set in WaitForChange().
      run_loop_->Quit();
  }

 private:
  std::vector<Change> observed_changes_;
  mojo::Binding<network::mojom::CookieChangeListener> binding_;

  // If not null, will be stopped when a cookie change notification is received.
  base::RunLoop* run_loop_ = nullptr;
};

}  // anonymous namespace

TEST_F(RestrictedCookieManagerTest, ChangeDispatch) {
  network::mojom::CookieChangeListenerPtr listener_ptr;
  network::mojom::CookieChangeListenerRequest request =
      mojo::MakeRequest(&listener_ptr);
  sync_service_->AddChangeListener(GURL("http://example.com/test/"),
                                   GURL("http://example.com"),
                                   std::move(listener_ptr));
  TestCookieChangeListener listener(std::move(request));

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(0));

  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  listener.WaitForChange();

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(network::mojom::CookieChangeCause::INSERTED,
            listener.observed_changes()[0].change_cause);
  EXPECT_EQ("cookie-name", listener.observed_changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value", listener.observed_changes()[0].cookie.Value());
}

TEST_F(RestrictedCookieManagerTest, AddChangeListenerFromWrongOrigin) {
  network::mojom::CookieChangeListenerPtr bad_listener_ptr;
  network::mojom::CookieChangeListenerRequest bad_request =
      mojo::MakeRequest(&bad_listener_ptr);
  ExpectBadMessage();
  sync_service_->AddChangeListener(GURL("http://not-example.com/test/"),
                                   GURL("http://not-example.com"),
                                   std::move(bad_listener_ptr));
  EXPECT_TRUE(received_bad_message());
  TestCookieChangeListener bad_listener(std::move(bad_request));

  network::mojom::CookieChangeListenerPtr good_listener_ptr;
  network::mojom::CookieChangeListenerRequest good_request =
      mojo::MakeRequest(&good_listener_ptr);
  sync_service_->AddChangeListener(GURL("http://example.com/test/"),
                                   GURL("http://example.com"),
                                   std::move(good_listener_ptr));
  TestCookieChangeListener good_listener(std::move(good_request));

  ASSERT_THAT(bad_listener.observed_changes(), testing::SizeIs(0));
  ASSERT_THAT(good_listener.observed_changes(), testing::SizeIs(0));

  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  good_listener.WaitForChange();

  EXPECT_THAT(bad_listener.observed_changes(), testing::SizeIs(0));

  ASSERT_THAT(good_listener.observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(network::mojom::CookieChangeCause::INSERTED,
            good_listener.observed_changes()[0].change_cause);
  EXPECT_EQ("cookie-name", good_listener.observed_changes()[0].cookie.Name());
  EXPECT_EQ("cookie-value", good_listener.observed_changes()[0].cookie.Value());
}

}  // namespace network
