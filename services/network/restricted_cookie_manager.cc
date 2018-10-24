// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"

namespace network {

namespace {

// TODO(pwnall): De-duplicate from cookie_manager.cc
mojom::CookieChangeCause ToCookieChangeCause(net::CookieChangeCause net_cause) {
  switch (net_cause) {
    case net::CookieChangeCause::INSERTED:
      return mojom::CookieChangeCause::INSERTED;
    case net::CookieChangeCause::EXPLICIT:
      return mojom::CookieChangeCause::EXPLICIT;
    case net::CookieChangeCause::UNKNOWN_DELETION:
      return mojom::CookieChangeCause::UNKNOWN_DELETION;
    case net::CookieChangeCause::OVERWRITE:
      return mojom::CookieChangeCause::OVERWRITE;
    case net::CookieChangeCause::EXPIRED:
      return mojom::CookieChangeCause::EXPIRED;
    case net::CookieChangeCause::EVICTED:
      return mojom::CookieChangeCause::EVICTED;
    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      return mojom::CookieChangeCause::EXPIRED_OVERWRITE;
  }
  NOTREACHED();
  return mojom::CookieChangeCause::EXPLICIT;
}

}  // anonymous namespace

class RestrictedCookieManager::Listener : public base::LinkNode<Listener> {
 public:
  Listener(net::CookieStore* cookie_store,
           const GURL& url,
           net::CookieOptions options,
           mojom::CookieChangeListenerPtr mojo_listener)
      : url_(url), options_(options), mojo_listener_(std::move(mojo_listener)) {
    // TODO(pwnall): add a constructor w/options to net::CookieChangeDispatcher.
    cookie_store_subscription_ =
        cookie_store->GetChangeDispatcher().AddCallbackForUrl(
            url,
            base::BindRepeating(
                &Listener::OnCookieChange,
                // Safe because net::CookieChangeDispatcher guarantees that the
                // callback will stop being called immediately after we remove
                // the subscription, and the cookie store lives on the same
                // thread as we do.
                base::Unretained(this)));
  }

  ~Listener() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  mojom::CookieChangeListenerPtr& mojo_listener() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mojo_listener_;
  }

 private:
  // net::CookieChangeDispatcher callback.
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      net::CookieChangeCause cause) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!cookie.IncludeForRequestURL(url_, options_))
      return;
    mojo_listener_->OnCookieChange(cookie, ToCookieChangeCause(cause));
  }

  // The CookieChangeDispatcher subscription used by this listener.
  std::unique_ptr<net::CookieChangeSubscription> cookie_store_subscription_;

  // The URL whose cookies this listener is interested in.
  const GURL url_;
  // CanonicalCookie::IncludeForRequestURL options for this listener's interest.
  const net::CookieOptions options_;

  mojom::CookieChangeListenerPtr mojo_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Listener);
};

RestrictedCookieManager::RestrictedCookieManager(net::CookieStore* cookie_store,
                                                 const url::Origin& origin)
    : cookie_store_(cookie_store), origin_(origin), weak_ptr_factory_(this) {
  DCHECK(cookie_store);
}

RestrictedCookieManager::~RestrictedCookieManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::LinkNode<Listener>* node = listeners_.head();
  while (node != listeners_.end()) {
    Listener* listener_reference = node->value();
    node = node->next();
    // The entire list is going away, no need to remove nodes from it.
    delete listener_reference;
  }
}

void RestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run({});
    return;
  }

  net::CookieOptions net_options;
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, site_for_cookies,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // TODO(mkwst): This check ought to further distinguish between frames
    // initiated in a strict or lax same-site context.
    net_options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  } else {
    net_options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE);
  }

  cookie_store_->GetCookieListWithOptionsAsync(
      url, net_options,
      base::BindOnce(&RestrictedCookieManager::CookieListToGetAllForUrlCallback,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     std::move(options), std::move(callback)));
}

void RestrictedCookieManager::CookieListToGetAllForUrlCallback(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback,
    const net::CookieList& cookie_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(pwnall): Call NetworkDelegate::CanGetCookies() on a NetworkDelegate
  //               associated with the NetworkContext.

  std::vector<net::CanonicalCookie> result;
  result.reserve(cookie_list.size());
  mojom::CookieMatchType match_type = options->match_type;
  const std::string& match_name = options->name;
  for (size_t i = 0; i < cookie_list.size(); ++i) {
    const net::CanonicalCookie& cookie = cookie_list[i];
    const std::string& cookie_name = cookie.Name();

    if (match_type == mojom::CookieMatchType::EQUALS) {
      if (cookie_name != match_name)
        continue;
    } else if (match_type == mojom::CookieMatchType::STARTS_WITH) {
      if (!base::StartsWith(cookie_name, match_name,
                            base::CompareCase::SENSITIVE)) {
        continue;
      }
    } else {
      NOTREACHED();
    }
    result.emplace_back(cookie);
  }
  std::move(callback).Run(std::move(result));
}

void RestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const GURL& site_for_cookies,
    SetCanonicalCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(pwnall): Validate the CanonicalCookie fields.

  // TODO(pwnall): Call NetworkDelegate::CanSetCookie() on a NetworkDelegate
  //               associated with the NetworkContext.
  base::Time now = base::Time::NowFromSystemTime();
  // TODO(pwnall): Reason about whether it makes sense to allow a renderer to
  //               set these fields.
  net::CookieSameSite cookie_same_site_mode = net::CookieSameSite::STRICT_MODE;
  net::CookiePriority cookie_priority = net::COOKIE_PRIORITY_DEFAULT;
  auto sanitized_cookie = std::make_unique<net::CanonicalCookie>(
      cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(), now,
      cookie.ExpiryDate(), now, cookie.IsSecure(), cookie.IsHttpOnly(),
      cookie_same_site_mode, cookie_priority);

  // TODO(pwnall): secure_source should depend on url, and might depend on the
  //               renderer.
  bool secure_source = true;
  bool modify_http_only = false;
  cookie_store_->SetCanonicalCookieAsync(std::move(sanitized_cookie),
                                         secure_source, modify_http_only,
                                         std::move(callback));
}

void RestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieChangeListenerPtr mojo_listener,
    AddChangeListenerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run();
    return;
  }

  net::CookieOptions net_options;
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, site_for_cookies,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // TODO(mkwst): This check ought to further distinguish between frames
    // initiated in a strict or lax same-site context.
    net_options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  } else {
    net_options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE);
  }

  auto listener = std::make_unique<Listener>(cookie_store_, url, net_options,
                                             std::move(mojo_listener));

  listener->mojo_listener().set_connection_error_handler(
      base::BindOnce(&RestrictedCookieManager::RemoveChangeListener,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Safe because this owns the listener, so the listener is
                     // guaranteed to be alive for as long as the weak pointer
                     // above resolves.
                     base::Unretained(listener.get())));

  // The linked list takes over the Listener ownership.
  listeners_.Append(listener.release());
  std::move(callback).Run();
}

void RestrictedCookieManager::RemoveChangeListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listener->RemoveFromList();
  delete listener;
}

bool RestrictedCookieManager::ValidateAccessToCookiesAt(const GURL& url) {
  if (origin_.IsSameOriginWith(url::Origin::Create(url)))
    return true;

  mojo::ReportBadMessage("Incorrect url origin");
  return false;
}

}  // namespace network
