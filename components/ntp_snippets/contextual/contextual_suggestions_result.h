// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_RESULT_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_RESULT_H_

#include "components/ntp_snippets/contextual/contextual_suggestion.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"

namespace contextual_suggestions {

// Encapsulates conditions under which to show or "peek" the contextual
// suggestions UI.
struct PeekConditions {
  PeekConditions();
  // A measure of confidence that auto-peek should be enabled for this response
  // in the range [0, 1].
  float confidence = 1.0;

  // The percentage of the page that the user scrolls required for an auto
  // peek to occur.
  float page_scroll_percentage;

  // The minimum time (seconds) the user spends on the page required for
  // auto peek.
  float minimum_seconds_on_page;

  // The maximum number of auto peeks that we can show for this page.
  uint64_t maximum_number_of_peeks;
};

// A structure representing a suggestion cluster.
struct Cluster {
  Cluster();
  Cluster(const Cluster&);
  Cluster(Cluster&&) noexcept;
  ~Cluster();

  Cluster& operator=(const Cluster&);

  std::string title;
  std::vector<ContextualSuggestion> suggestions;
};

// Allows concise construction of a cluster.
class ClusterBuilder {
 public:
  explicit ClusterBuilder(const std::string& title);

  // Allow copying for ease of validation when testing.
  ClusterBuilder(const ClusterBuilder& other);
  ClusterBuilder& AddSuggestion(ContextualSuggestion suggestion);
  Cluster Build();

 private:
  Cluster cluster_;
};

// Synthetic field trials driven by the server.
struct ServerExperimentInfo {
  ServerExperimentInfo();
  ServerExperimentInfo(std::string name, std::string group);
  ServerExperimentInfo(const ServerExperimentInfo&);
  ServerExperimentInfo(ServerExperimentInfo&&) noexcept;
  ~ServerExperimentInfo();

  ServerExperimentInfo& operator=(ServerExperimentInfo&&);
  ServerExperimentInfo& operator=(const ServerExperimentInfo&);

  std::string name;
  std::string group;
};

using ServerExperimentInfos = std::vector<ServerExperimentInfo>;

// Struct that holds the data from a ContextualSuggestions response that we care
// about for UI purposes.
struct ContextualSuggestionsResult {
  ContextualSuggestionsResult();
  ContextualSuggestionsResult(std::string peek_text,
                              std::vector<Cluster> clusters,
                              PeekConditions peek_conditions,
                              ServerExperimentInfos experiment_infos);
  ContextualSuggestionsResult(const ContextualSuggestionsResult&);
  ContextualSuggestionsResult(ContextualSuggestionsResult&&) noexcept;
  ~ContextualSuggestionsResult();

  ContextualSuggestionsResult& operator=(ContextualSuggestionsResult&&);
  ContextualSuggestionsResult& operator=(const ContextualSuggestionsResult&);

  std::vector<Cluster> clusters;
  std::string peek_text;
  PeekConditions peek_conditions;
  ServerExperimentInfos experiment_infos;
};

using FetchClustersCallback =
    base::OnceCallback<void(ContextualSuggestionsResult result)>;

}  // namespace contextual_suggestions

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_RESULT_H_
