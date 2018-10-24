// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_cache.h"

namespace contextual_suggestions {

ContextualSuggestionsCache::ContextualSuggestionsCache(size_t capacity)
    : cache_(capacity) {}
ContextualSuggestionsCache::~ContextualSuggestionsCache() = default;

base::flat_map<GURL, ContextualSuggestionsResult>
ContextualSuggestionsCache::GetAllCachedResultsForDebugging() {
  return base::flat_map<GURL, ContextualSuggestionsResult>(cache_.begin(),
                                                           cache_.end());
}

bool ContextualSuggestionsCache::GetSuggestionsResult(
    const GURL& url,
    ContextualSuggestionsResult* result) {
  auto cache_iter = cache_.Get(url);
  if (cache_iter == cache_.end()) {
    return false;
  }

  *result = cache_iter->second;
  return true;
}

void ContextualSuggestionsCache::AddSuggestionsResult(
    const GURL& url,
    ContextualSuggestionsResult result) {
  cache_.Put(url, result);
}

void ContextualSuggestionsCache::Clear() {
  cache_.Clear();
}

}  // namespace contextual_suggestions
