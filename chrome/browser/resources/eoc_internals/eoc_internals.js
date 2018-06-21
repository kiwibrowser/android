// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Reference to the backend.
let pageHandler = null;

(function() {
/* Utility functions */

/**
 * Clears children of the given domId.
 * @param {string} domId Id of the DOM element to be cleared.
 */
function clearChildrenForId(domId) {
  const parent = $(domId);
  while (parent.firstChild) {
    parent.removeChild(parent.firstChild);
  }
}

/* Backend wrapper functions */

// Get the general properties.
function updatePageWithProperties() {
  pageHandler.getProperties().then(response => {
    response.properties.forEach(function(value, field) {
      $(field).textContent = value;
    });
  });
}

// Get the cache of metric events, and add them to the page.
function updatePageWithCachedMetricEvents() {
  pageHandler.getCachedMetricEvents().then(response => {
    const metricContainerId = 'metric-events';
    clearChildrenForId(metricContainerId);
    addCachedMetricEventsToPage(response.metrics);
  });
}

// Add the metric events to the page.
function addCachedMetricEventsToPage(metrics) {
  const metricEventTemplate = document.querySelector('#metric-event-template');
  metrics.forEach(metricEvent => {
    const metricEventClone = metricEventTemplate.content.cloneNode(true);
    metricEventClone.querySelector('#url').textContent = metricEvent.url;
    metricEventClone.querySelector('#sheet-peeked').textContent =
        metricEvent.sheetPeeked;
    metricEventClone.querySelector('#sheet-opened').textContent =
        metricEvent.sheetOpened;
    metricEventClone.querySelector('#sheet-closed').textContent =
        metricEvent.sheetClosed;
    metricEventClone.querySelector('#any-suggestion-taken').textContent =
        metricEvent.anySuggestionTaken;
    metricEventClone.querySelector('#any-suggestion-downloaded').textContent =
        metricEvent.anySuggestionDownloaded;

    $('metric-events').appendChild(metricEventClone);
  });
}

// Get the cached suggestion results, and add them to the page.
function updatePageWithCachedSuggestionResults() {
  pageHandler.getCachedSuggestionResults().then(response => {
    const suggestionContainerId = 'suggestion-results';
    clearChildrenForId(suggestionContainerId);
    addCachedSuggestionResultsToPage(response.results);
  });
}

// Add the results to the page.
function addCachedSuggestionResultsToPage(results) {
  const suggestionResultTemplate =
      document.querySelector('#suggestion-result-template');
  results.forEach(result => {
    const resultClone = suggestionResultTemplate.content.cloneNode(true);
    resultClone.querySelector('#suggestion-result-url').textContent =
        result.url;
    resultClone.querySelector('#peek-confidence').textContent =
        result.peekConditions.confidence;
    resultClone.querySelector('#peek-page-scroll-percentage').textContent =
        result.peekConditions.pageScrollPercentage;
    resultClone.querySelector('#peek-min-seconds-on-page').textContent =
        result.peekConditions.minimumSecondsOnPage;
    resultClone.querySelector('#peek-max-peeks').textContent =
        result.peekConditions.maximumNumberOfPeeks;

    const suggestionTemplate = document.querySelector('#suggestion-template');
    result.suggestions.forEach(suggestion => {
      const suggestionClone = suggestionTemplate.content.cloneNode(true);
      suggestionClone.querySelector('#suggestion-title').textContent =
          suggestion.title;
      suggestionClone.querySelector('#suggestion-url').textContent =
          suggestion.url;
      suggestionClone.querySelector('#suggestion-title').textContent =
          suggestion.title + ' >>';
      suggestionClone.querySelector('#suggestion-title').onclick = function(
          event) {
        const suggestion = event.currentTarget.parentElement.querySelector(
            '#suggestion-details');
        suggestion.hidden = !suggestion.hidden;
      };
      suggestionClone.querySelector('#suggestions-publisher-name').textContent =
          suggestion.publisherName;
      suggestionClone.querySelector('#suggestion-snippet').textContent =
          suggestion.snippet;
      suggestionClone.querySelector('#suggestion-url').textContent =
          suggestion.url;

      resultClone.querySelector('#cached-suggestions')
          .appendChild(suggestionClone);
    });

    $('suggestion-results').appendChild(resultClone);
  });
}

// Clear the cache of metric events.
function clearCachedMetricEvents() {
  pageHandler.clearCachedMetricEvents().then(() => {
    clearChildrenForId('metric-events');
  });
}

// Clear the cached suggestion results from the server.
function clearCachedSuggestionResults() {
  pageHandler.clearCachedSuggestionResults().then(() => {
    clearChildrenForId('suggestion-results');
  });
}

function setupEventListeners() {
  $('clear-metrics').onclick = clearCachedMetricEvents;
  $('clear-suggestion-results').onclick = clearCachedSuggestionResults;
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = new eocInternals.mojom.PageHandlerPtr;
  Mojo.bindInterface(
      eocInternals.mojom.PageHandler.name,
      mojo.makeRequest(pageHandler).handle);

  updatePageWithProperties();
  updatePageWithCachedMetricEvents();
  updatePageWithCachedSuggestionResults();

  setupEventListeners();
});
})();