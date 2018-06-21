// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * cr-lazy-render is a simple variant of dom-if designed for lazy rendering
 * of elements that are accessed imperatively.
 * Usage:
 *   <cr-lazy-render id="menu">
 *     <template>
 *       <heavy-menu></heavy-menu>
 *     </template>
 *   </cr-lazy-render>
 *
 *   this.$.menu.get().show();
 *
 * TODO(calamity): Use Polymer.Templatize instead of inheriting the
 * Polymer.Templatizer behavior once Polymer 2.0 is available.
 */

Polymer({
  is: 'cr-lazy-render',

  behaviors: [Polymer.Templatizer],

  /** @private {?Element} */
  child_: null,

  /** @private {?Element} */
  instance_: null,

  /**
   * Stamp the template into the DOM tree synchronously
   * @return {Element} Child element which has been stamped into the DOM tree.
   */
  get: function() {
    if (!this.child_)
      this.render_();
    return this.child_;
  },

  /**
   * @return {?Element} The element contained in the template, if it has
   *   already been stamped.
   */
  getIfExists: function() {
    return this.child_;
  },

  /** @private */
  render_: function() {
    var template = this.getContentChildren()[0];
    if (!this.ctor)
      this.templatize(template);
    var parentNode = this.parentNode;
    if (parentNode && !this.child_) {
      this.instance_ = this.stamp({});
      this.child_ = this.instance_.root.firstElementChild;
      parentNode.insertBefore(this.instance_.root, this);
    }
  },

  /**
   * TODO(dpapad): Delete this method once migration to Polymer 2 has finished.
   * @param {string} prop
   * @param {Object} value
   */
  _forwardParentProp: function(prop, value) {
    if (this.child_)
      this.child_._templateInstance[prop] = value;
  },

  /**
   * TODO(dpapad): Delete this method once migration to Polymer 2 has finished.
   * @param {string} path
   * @param {Object} value
   */
  _forwardParentPath: function(path, value) {
    if (this.child_)
      this.child_._templateInstance.notifyPath(path, value, true);
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardHostPropV2: function(prop, value) {
    if (this.instance_)
      this.instance_.forwardHostProp(prop, value);
  },
});
