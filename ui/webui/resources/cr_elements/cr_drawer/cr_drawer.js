// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-drawer',

  properties: {
    heading: String,

    open: {
      type: Boolean,
      // Enables 'open-changed' events.
      notify: true,
    },

    /** The alignment of the drawer on the screen ('ltr' or 'rtl'). */
    align: {
      type: String,
      value: 'ltr',
      reflectToAttribute: true,
    },
  },

  /** Toggles the drawer open and close. */
  toggle: function() {
    if (this.$.dialog.open)
      this.closeDrawer();
    else
      this.openDrawer();
  },

  /** Shows drawer and slides it into view. */
  openDrawer: function() {
    if (!this.$.dialog.open) {
      this.$.dialog.showModal();
      this.open = true;
      this.classList.add('opening');
    }
  },

  /** Slides the drawer away, then closes it after the transition has ended. */
  closeDrawer: function() {
    if (this.$.dialog.open) {
      this.classList.remove('opening');
      this.classList.add('closing');
    }
  },

  /**
   * Stop propagation of a tap event inside the container. This will allow
   * |onDialogTap_| to only be called when clicked outside the container.
   * @param {!Event} event
   * @private
   */
  onContainerTap_: function(event) {
    event.stopPropagation();
  },

  /**
   * Close the dialog when tapped outside the container.
   * @private
   */
  onDialogTap_: function() {
    this.closeDrawer();
  },

  /**
   * Overrides the default cancel machanism to allow for a close animation.
   * @param {!Event} event
   * @private
   */
  onDialogCancel_: function(event) {
    event.preventDefault();
    this.closeDrawer();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDialogClose_: function(event) {
    // TODO(dpapad): This is necessary to make the code work both for Polymer 1
    // and Polymer 2. Remove once migration to Polymer 2 is completed.
    event.stopPropagation();

    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire('close');
  },

  /**
   * Closes the dialog when the closing animation is over.
   * @private
   */
  onDialogTransitionEnd_: function() {
    if (this.classList.contains('closing')) {
      this.classList.remove('closing');
      this.$.dialog.close();
      this.open = false;
    } else if (this.classList.contains('opening')) {
      this.fire('cr-drawer-opened');
    }
  },
});
