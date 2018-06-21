// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-expand-button' is a chrome-specific wrapper around a button that toggles
 * between an opened (expanded) and closed state.
 */
Polymer({
  is: 'cr-expand-button',

  properties: {
    /**
     * If true, the button is in the expanded state and will show the
     * 'expand-less' icon. If false, the button shows the 'expand-more' icon.
     */
    expanded: {type: Boolean, value: false, notify: true},

    /**
     * If true, the button will be disabled and grayed out.
     */
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    /** A11y text descriptor for this control. */
    alt: String,

    tabIndex: {
      type: Number,
      value: 0,
    },

  },

  listeners: {
    'blur': 'onBlur_',
    'click': 'toggleExpand_',
    'focus': 'onFocus_',
    'keypress': 'onKeyPress_',
    'pointerdown': 'onPointerDown_',
  },

  /**
   * Used to differentiate pointer and keyboard click events.
   * @private {boolean}
   */
  fromPointer_: false,

  /**
   * @param {boolean} expanded
   * @private
   */
  getAriaPressed_: function(expanded) {
    return expanded ? 'true' : 'false';
  },


  /**
   * @param {boolean} expanded
   * @private
   */
  iconName_: function(expanded) {
    return expanded ? 'icon-expand-less' : 'icon-expand-more';
  },

  /** @private */
  onBlur_: function() {
    this.updateRippleHoldDown_(false);
  },

  /** @private */
  onFocus_: function() {
    this.updateRippleHoldDown_(true);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeyPress_: function(event) {
    if (event.key == ' ' || event.key == 'Enter')
      this.updateRippleHoldDown_(true);
  },

  /** @private */
  onPointerDown_: function() {
    this.fromPointer_ = true;
  },

  /**
   * @param {!Event} event
   * @private
   */
  toggleExpand_: function(event) {
    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    event.stopPropagation();
    event.preventDefault();

    this.expanded = !this.expanded;

    // If this event originated from a pointer, then |ripple.holdDown| should
    // preemptively be set to false to allow ripple to animate.
    if (this.fromPointer_)
      this.updateRippleHoldDown_(false);
    this.fromPointer_ = false;
  },

  /**
   * @param {boolean} holdDown
   * @private
   */
  updateRippleHoldDown_: function(holdDown) {
    this.$$('paper-ripple').holdDown = holdDown;
  },
});
