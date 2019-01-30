// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('__crWeb.manualfill');

/* Beginning of anonymous object. */
(function() {

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.manualfill = {};
__gCrWeb['manualfill'] = __gCrWeb.manualfill;

/**
 * Returns the identifier to be used with `setValueForElementId`.
 *
 * @return {string} The id of the active element.
 */
__gCrWeb.manualfill.activeElementId = function() {
  var activeElementId = document.activeElement.id;
  return activeElementId;
};

/**
 * Sets the value passed for the element with the identifier passed.
 *
 * @param {string} value The input to set in the element with the identifier.
 * @param {string} identifier The identifier of the element, found with
 *                 `activeElementId`.
 */
__gCrWeb.manualfill.setValueForElementId = function(value, identifier) {
  if (!identifier) {
    return
  }
  var field = document.getElementById(identifier);
  if (!field) {
    return;
  }
  if (!value || value.length === 0) {
    return;
  }
  __gCrWeb.fill.setInputElementValue(value, field);
};

}());  // End of anonymous object
