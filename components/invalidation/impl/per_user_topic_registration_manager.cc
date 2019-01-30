// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_registration_manager.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/public/invalidation_object_id.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {

namespace {

const char kTypeRegisteredForInvalidation[] =
    "invalidation.registered_for_invalidation";

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kProjectId[] = "8181035976";

}  // namespace

// static
void PerUserTopicRegistrationManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTypeRegisteredForInvalidation);
}

struct PerUserTopicRegistrationManager::RegistrationEntry {
  enum RegistrationState { REGISTERED, UNREGISTERED };
  RegistrationEntry(const invalidation::InvalidationObjectId& id,
                    PrefService* pref);
  ~RegistrationEntry();

  void RegistrationFinished(const Status& code,
                            const std::string& private_topic_name);

  void DoRegister();

  // The object for which this is the status.
  const invalidation::InvalidationObjectId id;

  // Whether this data type should be registered.  Set to false if
  // we get a non-transient registration failure.
  bool enabled;
  // The current registration state.
  RegistrationState state;

  std::string private_topic_name;

  PrefService* pref;

  std::unique_ptr<PerUserTopicRegistrationRequest> request;

  DISALLOW_COPY_AND_ASSIGN(RegistrationEntry);
};

PerUserTopicRegistrationManager::RegistrationEntry::RegistrationEntry(
    const invalidation::InvalidationObjectId& id,
    PrefService* pref)
    : id(id),
      enabled(true),
      state(RegistrationEntry::UNREGISTERED),
      pref(pref) {
  const base::DictionaryValue* registered_for_invalidation =
      pref->GetDictionary(kTypeRegisteredForInvalidation);
  if (registered_for_invalidation &&
      registered_for_invalidation->FindKey(id.name())) {
    state = RegistrationEntry::REGISTERED;
  }
}

PerUserTopicRegistrationManager::RegistrationEntry::~RegistrationEntry() {}

void PerUserTopicRegistrationManager::RegistrationEntry::RegistrationFinished(
    const Status& code,
    const std::string& topic_name) {
  if (code.IsSuccess()) {
    private_topic_name = topic_name;
    state = RegistrationEntry::REGISTERED;
    ScopedUserPrefUpdate<base::DictionaryValue, base::Value::Type::DICTIONARY>
        topics_update(pref, kTypeRegisteredForInvalidation);
    topics_update->SetKey(id.name(), base::Value(private_topic_name));
  }
}

PerUserTopicRegistrationManager::PerUserTopicRegistrationManager(
    const std::string& instance_id_token,
    const std::string& access_token,
    PrefService* local_state,
    network::mojom::URLLoaderFactory* url_loader_factory,
    const ParseJSONCallback& parse_json)
    : local_state_(local_state),
      access_token_(access_token),
      token_(instance_id_token),
      parse_json_(parse_json),
      url_loader_factory_(url_loader_factory) {}

PerUserTopicRegistrationManager::~PerUserTopicRegistrationManager() {}

void PerUserTopicRegistrationManager::UpdateRegisteredIds(
    const InvalidationObjectIdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const invalidation::InvalidationObjectId& objectId : ids) {
    if (!base::ContainsKey(registration_statuses_, objectId)) {
      registration_statuses_[objectId] =
          std::make_unique<RegistrationEntry>(objectId, local_state_);
    }
    if (registration_statuses_[objectId]->state ==
        RegistrationEntry::UNREGISTERED) {
      TryToRegisterId(objectId);
    }
  }
}

void PerUserTopicRegistrationManager::TryToRegisterId(
    const invalidation::InvalidationObjectId& id) {
  auto it = registration_statuses_.find(id);
  if (it == registration_statuses_.end()) {
    NOTREACHED() << "TryToRegisterId called on "
                 << InvalidationObjectIdToString(id)
                 << " which is not in the registration map";
    return;
  }
  PerUserTopicRegistrationRequest::Builder builder;

  it->second->request =
      builder.SetToken(token_)
          .SetScope(kInvalidationRegistrationScope)
          .SetPublicTopicName(id.name())
          .SetAuthenticationHeader(base::StringPrintf(
              "%s: Bearer %s", net::HttpRequestHeaders::kAuthorization,
              access_token_.c_str()))
          .SetProjectId(kProjectId)
          .Build();

  it->second->request->Start(
      base::BindOnce(&PerUserTopicRegistrationManager::RegistrationEntry::
                         RegistrationFinished,
                     base::Unretained(it->second.get())),
      parse_json_, url_loader_factory_);
}

InvalidationObjectIdSet PerUserTopicRegistrationManager::GetRegisteredIds()
    const {
  InvalidationObjectIdSet ids;
  for (const auto& status_pair : registration_statuses_) {
    if (status_pair.second->state == RegistrationEntry::REGISTERED) {
      ids.insert(status_pair.first);
    }
  }
  return ids;
}

}  // namespace syncer
