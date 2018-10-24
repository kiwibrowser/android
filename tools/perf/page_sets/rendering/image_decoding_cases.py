# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ImageDecodingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.IMAGE_DECODING]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    if extra_browser_args is None:
      extra_browser_args = ['--disable-accelerated-jpeg-decoding']
    super(ImageDecodingPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('DecodeImage'):
      action_runner.Wait(5)


class YuvDecodingPage(ImageDecodingPage):
  BASE_NAME = 'yuv_decoding'
  URL = 'file://../image_decoding_cases/yuv_decoding.html'


# TODO(crbug.com/760553):remove this class after
# smoothness.image_decoding_cases benchmark is completely
# replaced by rendering benchmarks
class ImageDecodingCasesPageSet(story.StorySet):

  """ A directed benchmark of accelerated jpeg image decoding performance """

  def __init__(self):
    super(ImageDecodingCasesPageSet, self).__init__()

    self.AddStory(YuvDecodingPage(self))
