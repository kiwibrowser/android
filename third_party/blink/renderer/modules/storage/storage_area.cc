/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/storage/storage_area.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_storage_area.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/storage/dom_window_storage.h"
#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"
#include "third_party/blink/renderer/modules/storage/storage_event.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

StorageArea* StorageArea::Create(LocalFrame* frame,
                                 std::unique_ptr<WebStorageArea> storage_area,
                                 StorageType storage_type) {
  return new StorageArea(frame, std::move(storage_area), storage_type);
}

StorageArea::StorageArea(LocalFrame* frame,
                         std::unique_ptr<WebStorageArea> storage_area,
                         StorageType storage_type)
    : ContextClient(frame),
      storage_area_(std::move(storage_area)),
      storage_type_(storage_type) {
  DCHECK(frame);
  DCHECK(storage_area_);
}

unsigned StorageArea::length(ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError("access is denied for this document.");
    return 0;
  }
  return storage_area_->length();
}

String StorageArea::key(unsigned index, ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError("access is denied for this document.");
    return String();
  }
  bool did_decrease_iterator = false;
  String result = storage_area_->Key(index, &did_decrease_iterator);
  if (did_decrease_iterator)
    UseCounter::Count(GetFrame(), WebFeature::kReverseIterateDOMStorage);
  return result;
}

String StorageArea::getItem(const String& key,
                            ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError("access is denied for this document.");
    return String();
  }
  return storage_area_->GetItem(key);
}

bool StorageArea::setItem(const String& key,
                          const String& value,
                          ExceptionState& exception_state) {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError("access is denied for this document.");
    return true;
  }
  WebStorageArea::Result result = WebStorageArea::kResultOK;
  storage_area_->SetItem(key, value, GetFrame()->GetDocument()->Url(), result);
  if (result != WebStorageArea::kResultOK) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kQuotaExceededError,
        "Setting the value of '" + key + "' exceeded the quota.");
  }
  return true;
}

DeleteResult StorageArea::removeItem(const String& key,
                                     ExceptionState& exception_state) {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError("access is denied for this document.");
    return kDeleteSuccess;
  }
  storage_area_->RemoveItem(key, GetFrame()->GetDocument()->Url());
  return kDeleteSuccess;
}

void StorageArea::clear(ExceptionState& exception_state) {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError("access is denied for this document.");
    return;
  }
  storage_area_->Clear(GetFrame()->GetDocument()->Url());
}

bool StorageArea::Contains(const String& key,
                           ExceptionState& exception_state) const {
  if (!CanAccessStorage()) {
    exception_state.ThrowSecurityError("access is denied for this document.");
    return false;
  }
  return !storage_area_->GetItem(key).IsNull();
}

void StorageArea::NamedPropertyEnumerator(Vector<String>& names,
                                          ExceptionState& exception_state) {
  unsigned length = this->length(exception_state);
  if (exception_state.HadException())
    return;
  names.resize(length);
  for (unsigned i = 0; i < length; ++i) {
    String key = this->key(i, exception_state);
    if (exception_state.HadException())
      return;
    DCHECK(!key.IsNull());
    String val = getItem(key, exception_state);
    if (exception_state.HadException())
      return;
    names[i] = key;
  }
}

bool StorageArea::NamedPropertyQuery(const AtomicString& name,
                                     ExceptionState& exception_state) {
  if (name == "length")
    return false;
  bool found = Contains(name, exception_state);
  return found && !exception_state.HadException();
}

void StorageArea::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

bool StorageArea::CanAccessStorage() const {
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->GetPage())
    return false;

  if (did_check_can_access_storage_)
    return can_access_storage_cached_result_;
  StorageNamespaceController* controller =
      StorageNamespaceController::From(frame->GetPage());
  if (!controller)
    return false;
  can_access_storage_cached_result_ =
      controller->CanAccessStorageArea(frame, storage_type_);
  did_check_can_access_storage_ = true;
  return can_access_storage_cached_result_;
}

namespace {
Page* FindPageWithSessionStorageNamespace(
    const WebStorageNamespace& session_namespace) {
  // Iterate over all pages that have a StorageNamespaceController supplement.
  for (Page* page : Page::OrdinaryPages()) {
    const bool kDontCreateIfMissing = false;
    StorageNamespace* storage_namespace =
        StorageNamespaceController::From(page)->SessionStorage(
            kDontCreateIfMissing);
    if (storage_namespace &&
        storage_namespace->IsSameNamespace(session_namespace))
      return page;
  }
  return nullptr;
}

bool IsEventSource(StorageArea* storage, WebStorageArea* source_area_instance) {
  DCHECK(storage);
  WebStorageArea* web_area = storage->Area();
  return web_area == source_area_instance;
}
}  // namespace

void StorageArea::DispatchLocalStorageEvent(
    const String& key,
    const String& old_value,
    const String& new_value,
    const SecurityOrigin* security_origin,
    const KURL& page_url,
    WebStorageArea* source_area_instance) {
  // Iterate over all pages that have a StorageNamespaceController supplement.
  for (Page* page : Page::OrdinaryPages()) {
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      // Remote frames are cross-origin and do not need to be notified of
      // events.
      if (!frame->IsLocalFrame())
        continue;
      LocalFrame* local_frame = ToLocalFrame(frame);
      LocalDOMWindow* local_window = local_frame->DomWindow();
      StorageArea* storage =
          DOMWindowStorage::From(*local_window).OptionalLocalStorage();
      if (storage &&
          local_frame->GetDocument()->GetSecurityOrigin()->IsSameSchemeHostPort(
              security_origin) &&
          !IsEventSource(storage, source_area_instance)) {
        local_frame->DomWindow()->EnqueueWindowEvent(
            StorageEvent::Create(EventTypeNames::storage, key, old_value,
                                 new_value, page_url, storage));
      }
    }
    if (InspectorDOMStorageAgent* agent =
            StorageNamespaceController::From(page)->InspectorAgent()) {
      agent->DidDispatchDOMStorageEvent(key, old_value, new_value,
                                        StorageType::kLocalStorage,
                                        security_origin);
    }
  }
}

void StorageArea::DispatchSessionStorageEvent(
    const String& key,
    const String& old_value,
    const String& new_value,
    const SecurityOrigin* security_origin,
    const KURL& page_url,
    const WebStorageNamespace& session_namespace,
    WebStorageArea* source_area_instance) {
  Page* page = FindPageWithSessionStorageNamespace(session_namespace);
  if (!page)
    return;

  for (Frame* frame = page->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    // Remote frames are cross-origin and do not need to be notified of events.
    if (!frame->IsLocalFrame())
      continue;
    LocalFrame* local_frame = ToLocalFrame(frame);
    LocalDOMWindow* local_window = local_frame->DomWindow();
    StorageArea* storage =
        DOMWindowStorage::From(*local_window).OptionalSessionStorage();
    if (storage &&
        local_frame->GetDocument()->GetSecurityOrigin()->IsSameSchemeHostPort(
            security_origin) &&
        !IsEventSource(storage, source_area_instance)) {
      local_frame->DomWindow()->EnqueueWindowEvent(
          StorageEvent::Create(EventTypeNames::storage, key, old_value,
                               new_value, page_url, storage));
    }
  }
  if (InspectorDOMStorageAgent* agent =
          StorageNamespaceController::From(page)->InspectorAgent()) {
    agent->DidDispatchDOMStorageEvent(key, old_value, new_value,
                                      StorageType::kSessionStorage,
                                      security_origin);
  }
}

}  // namespace blink
