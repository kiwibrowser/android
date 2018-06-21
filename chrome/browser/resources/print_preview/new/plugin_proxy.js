// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');
/**
 * @typedef {{accessibility: Function,
 *            documentLoadComplete: Function,
 *            getHeight: Function,
 *            getHorizontalScrollbarThickness: Function,
 *            getPageLocationNormalized: Function,
 *            getVerticalScrollbarThickness: Function,
 *            getWidth: Function,
 *            getZoomLevel: Function,
 *            goToPage: Function,
 *            grayscale: Function,
 *            loadPreviewPage: Function,
 *            onload: Function,
 *            onPluginSizeChanged: Function,
 *            onScroll: Function,
 *            pageXOffset: Function,
 *            pageYOffset: Function,
 *            reload: Function,
 *            resetPrintPreviewMode: Function,
 *            sendKeyEvent: Function,
 *            setPageNumbers: Function,
 *            setPageXOffset: Function,
 *            setPageYOffset: Function,
 *            setZoomLevel: Function,
 *            fitToHeight: Function,
 *            fitToWidth: Function,
 *            zoomIn: Function,
 *            zoomOut: Function}}
 */
print_preview_new.PDFPlugin;

cr.define('print_preview_new', function() {
  'use strict';

  /**
   * An interface to the PDF plugin.
   */
  class PluginProxy {
    /**
     * Creates a new PluginProxy if the current instance is not set.
     * @return {!print_preview_new.PluginProxy} The singleton instance.
     */
    static getInstance() {
      if (instance == null)
        instance = new PluginProxy();
      return assert(instance);
    }

    /**
     * @param {!print_preview_new.PluginProxy} newInstance The PluginProxy
     *     instance to set for print preview construction.
     */
    static setInstance(newInstance) {
      instance = newInstance;
    }

    constructor() {
      /** @private {?print_preview_new.PDFPlugin} */
      this.plugin_ = null;
    }

    /**
     * @param {!Element} oopCompatObj The out of process compatibility element.
     * @return {boolean} Whether the plugin exists and is compatible.
     */
    checkPluginCompatibility(oopCompatObj) {
      const isOOPCompatible = oopCompatObj.postMessage;
      oopCompatObj.parentElement.removeChild(oopCompatObj);

      return isOOPCompatible;
    }

    /** @return {boolean} Whether the plugin is ready. */
    pluginReady() {
      return !!this.plugin_;
    }

    /**
     * Creates the PDF plugin.
     * @param {number} previewUid The unique ID of the preview UI.
     * @param {number} index The preview index to load.
     * @return {!print_preview_new.PDFPlugin} The created plugin.
     */
    createPlugin(previewUid, index) {
      assert(!this.plugin_);
      const srcUrl = this.getPreviewUrl_(previewUid, index);
      this.plugin_ = /** @type {print_preview_new.PDFPlugin} */ (
          PDFCreateOutOfProcessPlugin(srcUrl, 'chrome://print/pdf'));
      this.plugin_.classList.add('preview-area-plugin');
      this.plugin_.setAttribute('aria-live', 'polite');
      this.plugin_.setAttribute('aria-atomic', 'true');
      // NOTE: The plugin's 'id' field must be set to 'pdf-viewer' since
      // chrome/renderer/printing/print_render_frame_helper.cc actually
      // references it.
      this.plugin_.setAttribute('id', 'pdf-viewer');
      return this.plugin_;
    }

    /**
     * Get the URL for the plugin.
     * @param {number} previewUid Unique identifier of preview.
     * @param {number} index Page index for plugin.
     * @return {string} The URL
     * @private
     */
    getPreviewUrl_(previewUid, index) {
      return `chrome://print/${previewUid}/${index}/print.pdf`;
    }

    /**
     * @param {number} previewUid Unique identifier of preview.
     * @param {number} index Page index for plugin.
     * @param {boolean} color Whether the preview should be color.
     * @param {!Array<number>} pages Page indices to preview.
     * @param {boolean} modifiable Whether the document is modifiable.
     */
    resetPrintPreviewMode(previewUid, index, color, pages, modifiable) {
      this.plugin_.resetPrintPreviewMode(
          this.getPreviewUrl_(previewUid, index), color, pages, modifiable);
    }

    /** @param {!KeyboardEvent} e Keyboard event to forward to the plugin. */
    sendKeyEvent(e) {
      this.plugin_.sendKeyEvent(e);
    }

    /**
     * @param {boolean} eventsEnabled Whether pointer events should be captured
     *     by the plugin.
     */
    setPointerEvents(eventsEnabled) {
      this.plugin_.style.pointerEvents = eventsEnabled ? 'auto' : 'none';
    }

    /**
     * @param {number} previewUid The unique ID of the preview UI.
     * @param {number} pageIndex The page index to load.
     * @param {number} index The preview index.
     */
    loadPreviewPage(previewUid, pageIndex, index) {
      this.plugin_.loadPreviewPage(
          this.getPreviewUrl_(previewUid, pageIndex), index);
    }
  }

  /** @type {?print_preview_new.PluginProxy} */
  let instance = null;

  // Export
  return {PluginProxy: PluginProxy};
});
