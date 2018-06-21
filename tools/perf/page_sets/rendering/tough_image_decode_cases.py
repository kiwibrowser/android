# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughImageDecodePage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_IMAGE_DECODE]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(ToughImageDecodePage, self).__init__(
      page_set=page_set,
      shared_page_state_class=shared_page_state_class,
      name_suffix=name_suffix,
      extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
      'document.readyState === "complete"')
    action_runner.ScrollPage(direction='down', speed_in_pixels_per_second=5000)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(direction='up', speed_in_pixels_per_second=5000)


class UnscaledImageDecodePage(ToughImageDecodePage):
  BASE_NAME = 'cats_unscaled'
  URL = 'http://localhost:9000/cats-unscaled.html'


class ViewPortWidthImageDecodePage(ToughImageDecodePage):
  BASE_NAME = 'cats_viewport_width'
  URL = 'http://localhost:9000/cats-viewport-width.html'


# TODO(crbug.com/760553):remove this class after
# smoothness.tough_image_decode_cases benchmark is completely
# replaced by rendering benchmarks
class ToughImageDecodeCasesPageSet(story.StorySet):

  """
  Description: A collection of difficult image decode tests
  """

  def __init__(self):
    super(ToughImageDecodeCasesPageSet, self).__init__(
      archive_data_file='../data/tough_image_decode_cases.json',
      cloud_storage_bucket=story.PUBLIC_BUCKET)

    page_classes = [
      UnscaledImageDecodePage,
      ViewPortWidthImageDecodePage
    ]

    for page_class in page_classes:
      self.AddStory(page_class(self))
