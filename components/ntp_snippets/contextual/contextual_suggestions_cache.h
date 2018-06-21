// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_CACHE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_CACHE_H_

#include "base/containers/flat_map.h"
#include "base/containers/mru_cache.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_result.h"
#include "url/gurl.h"

namespace contextual_suggestions {

// Wrapper for an LRU cache of ContextualSuggestionResult objects, keyed by
// context URL.
class ContextualSuggestionsCache {
 public:
  explicit ContextualSuggestionsCache(size_t capacity);
  ~ContextualSuggestionsCache();

  // Attempts to find a result for |url| in this cache, returning true and
  // putting the result into |result| if successful.
  bool GetSuggestionsResult(const GURL& url,
                            ContextualSuggestionsResult* result);
  // Adds |result| to this cache for the key |url|, overwriting any previous
  // value associated with |url| and potentially evicting the oldest item in the
  // cache.
  void AddSuggestionsResult(const GURL& url,
                            ContextualSuggestionsResult result);
  // Removes all items from the cache.
  void Clear();

  // Returns all suggestion results for debugging purposes.
  base::flat_map<GURL, ContextualSuggestionsResult>
  GetAllCachedResultsForDebugging();

 private:
  base::MRUCache<GURL, ContextualSuggestionsResult> cache_;
};

}  // namespace contextual_suggestions

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_CACHE_H_
