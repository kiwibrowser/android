// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_MANAGER_H_
#define SERVICES_NETWORK_COOKIE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_deletion_info.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace net {
class CookieStore;
}

class GURL;

namespace network {
class SessionCleanupCookieStore;
class SessionCleanupChannelIDStore;

// Wrap a cookie store in an implementation of the mojo cookie interface.

// This is an IO thread object; all methods on this object must be called on
// the IO thread.  Note that this does not restrict the locations from which
// mojo messages may be sent to the object.
class COMPONENT_EXPORT(NETWORK_SERVICE) CookieManager
    : public mojom::CookieManager {
 public:
  // Construct a CookieService that can serve mojo requests for the underlying
  // cookie store.  |*cookie_store| must outlive this object.
  CookieManager(
      net::CookieStore* cookie_store,
      scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store,
      scoped_refptr<SessionCleanupChannelIDStore>
          session_cleanup_channel_id_store);

  ~CookieManager() override;

  const CookieSettings& cookie_settings() const { return cookie_settings_; }

  // Bind a cookie request to this object.  Mojo messages
  // coming through the associated pipe will be served by this object.
  void AddRequest(mojom::CookieManagerRequest request);

  // TODO(rdsmith): Add a verion of AddRequest that does renderer-appropriate
  // security checks on bindings coming through that interface.

  // mojom::CookieManager
  void GetAllCookies(GetAllCookiesCallback callback) override;
  void GetCookieList(const GURL& url,
                     const net::CookieOptions& cookie_options,
                     GetCookieListCallback callback) override;
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool modify_http_only,
                          SetCanonicalCookieCallback callback) override;
  void DeleteCanonicalCookie(const net::CanonicalCookie& cookie,
                             DeleteCanonicalCookieCallback callback) override;
  void SetContentSettings(const ContentSettingsForOneType& settings) override;
  void DeleteCookies(mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override;
  void AddCookieChangeListener(
      const GURL& url,
      const std::string& name,
      mojom::CookieChangeListenerPtr listener) override;
  void AddGlobalChangeListener(
      mojom::CookieChangeListenerPtr listener) override;
  void CloneInterface(mojom::CookieManagerRequest new_interface) override;

  size_t GetClientsBoundForTesting() const { return bindings_.size(); }
  size_t GetListenersRegisteredForTesting() const {
    return listener_registrations_.size();
  }

  void FlushCookieStore(FlushCookieStoreCallback callback) override;
  void SetForceKeepSessionState() override;
  void BlockThirdPartyCookies(bool block) override;

 private:
  // State associated with a CookieChangeListener.
  struct ListenerRegistration {
    ListenerRegistration();
    ~ListenerRegistration();

    // Translates a CookieStore change callback to a CookieChangeListener call.
    void DispatchCookieStoreChange(const net::CanonicalCookie& cookie,
                                   net::CookieChangeCause cause);

    // Owns the callback registration in the store.
    std::unique_ptr<net::CookieChangeSubscription> subscription;

    // The observer receiving change notifications.
    mojom::CookieChangeListenerPtr listener;

    DISALLOW_COPY_AND_ASSIGN(ListenerRegistration);
  };

  // Handles connection errors on change listener pipes.
  void RemoveChangeListener(ListenerRegistration* registration);

  net::CookieStore* const cookie_store_;
  scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store_;
  scoped_refptr<SessionCleanupChannelIDStore> session_cleanup_channel_id_store_;
  mojo::BindingSet<mojom::CookieManager> bindings_;
  std::vector<std::unique_ptr<ListenerRegistration>> listener_registrations_;
  CookieSettings cookie_settings_;

  DISALLOW_COPY_AND_ASSIGN(CookieManager);
};

COMPONENT_EXPORT(NETWORK_SERVICE)
net::CookieDeletionInfo DeletionFilterToInfo(
    mojom::CookieDeletionFilterPtr filter);

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_MANAGER_H_
