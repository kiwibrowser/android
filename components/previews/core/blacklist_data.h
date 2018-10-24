// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_BLACKLIST_DATA_H_
#define COMPONENTS_PREVIEWS_CORE_BLACKLIST_DATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list_item.h"

namespace previews {

// The various reasons the Blacklist may tell that the user is blacklisted.
enum class BlacklistReason {
  // The blacklist may not be loaded very early in the session or when the user
  // has cleared the blacklist history (usually by clearing their browsing
  // history).
  kBlacklistNotLoaded,
  kUserOptedOutInSession,
  kUserOptedOutInGeneral,
  kUserOptedOutOfHost,
  kUserOptedOutOfType,
  kAllowed
};

// This class describes all of the data used to determine whether an action is
// allowed based on four possible rules: Session: if the user has opted out
// of j of the last k entries this session, the action will be blacklisted for a
// set duration. Persistent: if the user has opted out of j of the last k
// entries, the action will be blacklisted for a set duration. Host: if the user
// has opted out of threshold of the last history entries for a specific host,
// the action will be blacklisted for a set duration. Type: if the user has
// opted out of j of the last k entries for a specific type, the action will be
// blacklisted for a set duration. This is the in-memory version of the black
// list policy. This object is moved from the embedder thread to a background
// thread, It is not safe to access concurrently on two threads.
class BlacklistData {
 public:
  // A struct describing the general blacklisting pattern used by all of the
  // blacklisting rules.
  // The most recent |history| entries are looked at and if |threshold| (or
  // more) of them are opt outs, new actions are considered blacklisted unless
  // the most recent opt out was longer than |duration| ago.
  struct Policy {
    Policy(base::TimeDelta duration, size_t history, int threshold)
        : duration(duration), history(history), threshold(threshold) {}

    ~Policy() = default;

    // Specifies how long the blacklisting rule lasts after the most recent opt
    // out.
    const base::TimeDelta duration;
    // Amount of entries evaluated for the rule.
    const size_t history;
    // The number of opt outs that will trigger blacklisting for the rule.
    const int threshold;
  };

  // A map of types that are allowed to be used in the blacklist as well as the
  // version that those types are in. Versioning allows removals from persistent
  // memory at session start.
  using AllowedTypesAndVersions = std::map<int, int>;

  // |session_policy| if non-null, is the policy that is not persisted across
  // sessions and is not specific to host or type. |persistent_policy| if
  // non-null, is the policy that is persisted across sessions and is not
  // specific to host or type. |host_policy| if non-null, is the policy that is
  // persisted across sessions applies at the per-host level. |host_policy| if
  // non-null, is the policy that is persisted across sessions and applies at
  // the per-type level. |max_hosts| is the maximum number of hosts stored in
  // memory. |allowed_types| contains the action types that are allowed in the
  // session and their corresponding versions. Conversioning is used to clear
  // stale data from the persistent storage.
  BlacklistData(std::unique_ptr<Policy> session_policy,
                std::unique_ptr<Policy> persistent_policy,
                std::unique_ptr<Policy> host_policy,
                std::unique_ptr<Policy> type_policy,
                size_t max_hosts,
                AllowedTypesAndVersions allowed_types);
  ~BlacklistData();

  // Adds a new entry for all rules to use when evaluating blacklisting state.
  // |is_from_persistent_storage| is used to delineate between data added from
  // this session, and previous sessions.
  void AddEntry(const std::string& host_name,
                bool opt_out,
                int type,
                base::Time time,
                bool is_from_persistent_storage);

  // Whether the user is opted out when considering all enabled rules. if
  // |ignore_long_term_black_list_rules| is true, this will only check the
  // session rule. For every reason that is checked, but does not trigger
  // blacklisting, a new reason will be appended to the end |passed_reasons|.
  // |time| is the time that decision should be evaluated at (usually now).
  BlacklistReason IsAllowed(const std::string& host_name,
                            int type,
                            bool ignore_long_term_black_list_rules,
                            base::Time time,
                            std::vector<BlacklistReason>* passed_reasons) const;

  // This clears all data in all rules.
  void ClearData();

  // The allowed types and what version they are. If it is non-empty, it is used
  // to remove stale entries from the database and to DCHECK that other methods
  // are not using disallowed types.
  const AllowedTypesAndVersions& allowed_types() const {
    return allowed_types_;
  }

  // Whether the specific |host_name| is blacklisted based only on the host
  // rule.
  bool IsHostBlacklisted(const std::string& host_name, base::Time time) const;

  // Whether the user is opted out based solely on the persistent blacklist
  // rule.
  bool IsUserOptedOutInGeneral(base::Time time) const;

  // Exposed for logging purposes only.
  const std::map<std::string, PreviewsBlackListItem>& black_list_item_host_map()
      const {
    return black_list_item_host_map_;
  }

 private:
  // Removes the oldest (or safest) host item from |black_list_item_host_map_|.
  // Oldest is defined by most recent opt out time, and safest is defined as an
  // item with no opt outs.
  void EvictOldestHost();

  // The session rule policy. If non-null the session rule is enforced.
  std::unique_ptr<Policy> session_policy_;
  // The session rule history.
  std::unique_ptr<PreviewsBlackListItem> session_black_list_item_;

  // The persistent rule policy. If non-null the persistent rule is enforced.
  std::unique_ptr<Policy> persistent_policy_;
  // The persistent rule history.
  std::unique_ptr<PreviewsBlackListItem> persistent_black_list_item_;

  // The host rule policy. If non-null the host rule is enforced.
  std::unique_ptr<Policy> host_policy_;
  // The maximum number of hosts allowed in the host blacklist.
  size_t max_hosts_;
  // The host rule history. Each host is stored as a separate blacklist history.
  std::map<std::string, PreviewsBlackListItem> black_list_item_host_map_;

  // The type rule policy. If non-null the type rule is enforced.
  std::unique_ptr<Policy> type_policy_;
  // The type rule history. Each type is stored as a separate blacklist history.
  std::map<int, PreviewsBlackListItem> black_list_item_type_map_;

  // The allowed types and what version they are. If it is non-empty, it is used
  // to remove stale entries from the database and to DCHECK that other methods
  // are not using disallowed types.
  AllowedTypesAndVersions allowed_types_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistData);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_BLACKLIST_DATA_H_
