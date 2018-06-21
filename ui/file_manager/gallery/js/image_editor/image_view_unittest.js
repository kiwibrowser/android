// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testImageView() {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockEntry(mockFileSystem, '/test.jpg');

  // Item has full size cache.
  var itemWithFullCache = new GalleryItem(mockEntry, null, {}, null, false);
  itemWithFullCache.contentImage = document.createElement('canvas');
  assertEquals(
      ImageView.LoadTarget.CACHED_MAIN_IMAGE,
      ImageView.getLoadTarget(itemWithFullCache, new ImageView.Effect.None()));

  // Item with content thumbnail.
  var itemWithContentThumbnail = new GalleryItem(
      mockEntry, null, {thumbnail: {url: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithContentThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail.
  var itemWithExternalThumbnail = new GalleryItem(
      mockEntry, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail but present localy.
  var itemWithExternalThumbnailPresent = new GalleryItem(
      mockEntry, null, {external: {thumbnailUrl: 'url', present: true}}, null,
      false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailPresent, new ImageView.Effect.None()));

  // Item with external thumbnail shown by slide effect.
  var itemWithExternalThumbnailSlide = new GalleryItem(
      mockEntry, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailSlide, new ImageView.Effect.Slide(1)));

  // Item with external thumbnail shown by zoom to screen effect.
  var itemWithExternalThumbnailZoomToScreen = new GalleryItem(
      mockEntry, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailZoomToScreen,
          new ImageView.Effect.ZoomToScreen(new ImageRect(0, 0, 100, 100))));

  // Item with external thumbnail shown by zoom effect.
  var itemWithExternalThumbnailZoom = new GalleryItem(
      mockEntry, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailZoom,
          new ImageView.Effect.Zoom(0, 0, null)));

  // Item without cache/thumbnail.
  var itemWithoutCacheOrThumbnail = new GalleryItem(
      mockEntry, null, {}, null, false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithoutCacheOrThumbnail, new ImageView.Effect.None));
}

function testLoadVideo(callback) {
  var container = document.createElement('div');
  // We re-use the image-container for video, it starts with this class.
  container.classList.add('image-container');

  var viewport = new Viewport(window);

  // Mock volume manager.
  var volumeManager = {
    getVolumeInfo: function(entry) {
      return {volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS};
    }
  };

  var metadataModel = new MetadataModel(volumeManager);
  var imageView = new ImageView(container, viewport, metadataModel);

  var downloads = new MockFileSystem('file:///downloads');
  var getGalleryItem = function(path) {
    return new GalleryItem(
        new MockEntry(downloads, path), {isReadOnly: false}, {size: 100}, {},
        true /* original */);
  };
  var item = getGalleryItem('/test.webm');
  var effect = new ImageView.Effect.None();
  var displayDone = function() {
    assertTrue(container.classList.contains('image-container'));
    assertTrue(container.classList.contains('video-container'));
    var video = container.firstElementChild;
    assertTrue(video instanceof HTMLVideoElement);
    var source = video.firstElementChild;
    assertTrue(source instanceof HTMLSourceElement);
    assertTrue(source.src.startsWith(
        'filesystem:file:///downloads/test.webm?nocache='));
    callback();
  };
  var loadDone = function() {};
  imageView.load(item, effect, displayDone, loadDone);
}
