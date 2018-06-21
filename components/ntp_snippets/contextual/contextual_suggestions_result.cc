// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_result.h"

#include "components/ntp_snippets/contextual/contextual_suggestion.h"

namespace contextual_suggestions {

PeekConditions::PeekConditions()
    : confidence(1.0),
      page_scroll_percentage(0.0),
      minimum_seconds_on_page(0.0),
      maximum_number_of_peeks(0.0) {}

Cluster::Cluster() = default;

Cluster::Cluster(const Cluster& other) = default;

// MSVC doesn't support defaulted move constructors, so we have to define it
// ourselves.
Cluster::Cluster(Cluster&& other) noexcept
    : title(std::move(other.title)),
      suggestions(std::move(other.suggestions)) {}

Cluster::~Cluster() = default;

Cluster& Cluster::operator=(const Cluster&) = default;

ClusterBuilder::ClusterBuilder(const std::string& title) {
  cluster_.title = title;
}

ClusterBuilder::ClusterBuilder(const ClusterBuilder& other) {
  cluster_.title = other.cluster_.title;
  for (const auto& suggestion : other.cluster_.suggestions) {
    AddSuggestion(SuggestionBuilder(suggestion.url)
                      .Title(suggestion.title)
                      .PublisherName(suggestion.publisher_name)
                      .Snippet(suggestion.snippet)
                      .ImageId(suggestion.image_id)
                      .FaviconImageId(suggestion.favicon_image_id)
                      .FaviconImageUrl(suggestion.favicon_image_url)
                      .Build());
  }
}

ClusterBuilder& ClusterBuilder::AddSuggestion(ContextualSuggestion suggestion) {
  cluster_.suggestions.emplace_back(std::move(suggestion));
  return *this;
}

Cluster ClusterBuilder::Build() {
  return std::move(cluster_);
}

ServerExperimentInfo::ServerExperimentInfo() = default;

ServerExperimentInfo::ServerExperimentInfo(std::string name, std::string group)
    : name(name), group(group) {}

ServerExperimentInfo::ServerExperimentInfo(const ServerExperimentInfo& other) =
    default;

ServerExperimentInfo::ServerExperimentInfo(
    ServerExperimentInfo&& other) noexcept
    : name(std::move(other.name)), group(std::move(other.group)) {}

ServerExperimentInfo::~ServerExperimentInfo() = default;

ServerExperimentInfo& ServerExperimentInfo::operator=(
    ServerExperimentInfo&& other) = default;

ServerExperimentInfo& ServerExperimentInfo::operator=(
    const ServerExperimentInfo& other) = default;

ContextualSuggestionsResult::ContextualSuggestionsResult() = default;

ContextualSuggestionsResult::ContextualSuggestionsResult(
    std::string peek_text,
    std::vector<Cluster> clusters,
    PeekConditions peek_conditions,
    ServerExperimentInfos experiment_infos)
    : clusters(clusters),
      peek_text(peek_text),
      peek_conditions(peek_conditions),
      experiment_infos(std::move(experiment_infos)) {}

ContextualSuggestionsResult::ContextualSuggestionsResult(
    const ContextualSuggestionsResult& other) = default;

// MSVC doesn't support defaulted move constructors, so we have to define it
// ourselves.
ContextualSuggestionsResult::ContextualSuggestionsResult(
    ContextualSuggestionsResult&& other) noexcept
    : clusters(std::move(other.clusters)),
      peek_text(std::move(other.peek_text)),
      peek_conditions(std::move(other.peek_conditions)),
      experiment_infos(std::move(other.experiment_infos)) {}

ContextualSuggestionsResult::~ContextualSuggestionsResult() = default;

ContextualSuggestionsResult& ContextualSuggestionsResult::operator=(
    ContextualSuggestionsResult&& other) = default;

ContextualSuggestionsResult& ContextualSuggestionsResult::operator=(
    const ContextualSuggestionsResult& other) = default;

}  // namespace contextual_suggestions
