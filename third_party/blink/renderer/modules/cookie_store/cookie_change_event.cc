// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"

#include "third_party/blink/renderer/core/dom/dom_time_stamp.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event_init.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_list_item.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CookieChangeEvent::~CookieChangeEvent() = default;

const AtomicString& CookieChangeEvent::InterfaceName() const {
  return EventNames::CookieChangeEvent;
}

void CookieChangeEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
  visitor->Trace(changed_);
  visitor->Trace(deleted_);
}

CookieChangeEvent::CookieChangeEvent() = default;

CookieChangeEvent::CookieChangeEvent(const AtomicString& type,
                                     HeapVector<CookieListItem> changed,
                                     HeapVector<CookieListItem> deleted)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      changed_(std::move(changed)),
      deleted_(std::move(deleted)) {}

CookieChangeEvent::CookieChangeEvent(const AtomicString& type,
                                     const CookieChangeEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasChanged())
    changed_ = initializer.changed();
  if (initializer.hasDeleted())
    deleted_ = initializer.deleted();
}

// static
void CookieChangeEvent::ToCookieListItem(
    const WebCanonicalCookie& canonical_cookie,
    bool is_deleted,  // True for the information from a cookie deletion event.
    CookieListItem& list_item) {
  list_item.setName(canonical_cookie.Name());
  list_item.setPath(canonical_cookie.Path());
  list_item.setSecure(canonical_cookie.IsSecure());

  // The domain of host-only cookies is the host name, without a dot (.) prefix.
  String cookie_domain = canonical_cookie.Domain();
  if (cookie_domain.StartsWith("."))
    list_item.setDomain(cookie_domain.Substring(1));

  if (!is_deleted) {
    list_item.setValue(canonical_cookie.Value());
    if (!canonical_cookie.ExpiryDate().is_null()) {
      list_item.setExpires(ConvertSecondsToDOMTimeStamp(
          canonical_cookie.ExpiryDate().ToDoubleT()));
    }
  }
}

// static
void CookieChangeEvent::ToEventInfo(
    const WebCanonicalCookie& backend_cookie,
    ::network::mojom::CookieChangeCause change_cause,
    HeapVector<CookieListItem>& changed,
    HeapVector<CookieListItem>& deleted) {
  switch (change_cause) {
    case ::network::mojom::CookieChangeCause::INSERTED:
    case ::network::mojom::CookieChangeCause::EXPLICIT: {
      CookieListItem& cookie = changed.emplace_back();
      ToCookieListItem(backend_cookie, false /* is_deleted */, cookie);
      break;
    }
    case ::network::mojom::CookieChangeCause::UNKNOWN_DELETION:
    case ::network::mojom::CookieChangeCause::EXPIRED:
    case ::network::mojom::CookieChangeCause::EVICTED:
    case ::network::mojom::CookieChangeCause::EXPIRED_OVERWRITE: {
      CookieListItem& cookie = deleted.emplace_back();
      ToCookieListItem(backend_cookie, true /* is_deleted */, cookie);
      break;
    }

    case ::network::mojom::CookieChangeCause::OVERWRITE:
      // A cookie overwrite causes an OVERWRITE (meaning the old cookie was
      // deleted) and an INSERTED.
      break;
  }
}

}  // namespace blink
