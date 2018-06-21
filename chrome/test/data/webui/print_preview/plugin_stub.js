// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  /**
   * Test version of the PluginProxy.
   */
  class PDFPluginStub {
    constructor() {
      /** @type {?Function} The callback to call on load. */
      this.loadCallback_ = null;

      /** @type {boolean} Whether the plugin is compatible. */
      this.compatible_ = true;
    }

    /** @param {boolean} Whether the PDF plugin should be compatible. */
    setPluginCompatible(compatible) {
      this.compatible_ = compatible;
    }

    /**
     * @param {!Function} loadCallback Callback to call when the preview
     *     loads.
     */
    setLoadCallback(loadCallback) {
      this.loadCallback_ = loadCallback;
    }

    /**
     * @param {!Element} oopCompatObj The out of process compatibility element.
     * @return {boolean} Whether the plugin exists and is compatible.
     */
    checkPluginCompatibility(oopCompatObj) {
      return this.compatible_;
    }

    /** @return {boolean} Whether the plugin is ready. */
    pluginReady() {
      return true;
    }

    /**
     * Sets the load callback to imitate the plugin.
     * @param {number} previewUid The unique ID of the preview UI.
     * @param {number} index The preview index to load.
     * @return {?print_preview_new.PDFPlugin}
     */
    createPlugin(previewUid, index) {
      return null;
    }

    /**
     * @param {number} previewUid Unique identifier of preview.
     * @param {number} index Page index for plugin.
     * @param {boolean} color Whether the preview should be color.
     * @param {!Array<number>} pages Page indices to preview.
     * @param {boolean} modifiable Whether the document is modifiable.
     */
    resetPrintPreviewMode(previewUid, index, color, pages, modifiable) {}

    /** @param {!KeyEvent} e Keyboard event to forward to the plugin. */
    sendKeyEvent(e) {}

    /**
     * @param {boolean} eventsOn Whether pointer events should be captured by
     *     the plugin.
     */
    setPointerEvents(eventsOn) {}

    /**
     * Called when the preview area wants the plugin to load a preview page.
     * Immediately calls loadCallback_().
     * @param {number} previewUid The unique ID of the preview UI.
     * @param {number} pageIndex The page index to load.
     * @param {number} index The preview index.
     */
    loadPreviewPage(previewUid, pageIndex, index) {
      if (this.loadCallback_)
        this.loadCallback_();
    }
  }

  return {PDFPluginStub: PDFPluginStub};
});
