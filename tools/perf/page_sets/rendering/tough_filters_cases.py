# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughFiltersCasesPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_FILTERS]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(ToughFiltersCasesPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('Filter'):
      action_runner.Wait(10)


class MotionMarkPage(ToughFiltersCasesPage):
  BASE_NAME = 'motion_mark_focus'
  # pylint: disable=line-too-long
  URL = 'http://rawgit.com/WebKit/webkit/master/PerformanceTests/Animometer/developer.html?test-interval=20&display=minimal&controller=fixed&frame-rate=50&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Animometer&test-name=Focus&complexity=100'


class FilterTerrainSVGPage(ToughFiltersCasesPage):
  BASE_NAME = 'filter_terrain_svg'
  URL = 'http://letmespellitoutforyou.com/samples/svg/filter_terrain.svg'


class AnalogClockSVGPage(ToughFiltersCasesPage):
  BASE_NAME = 'analog_clock_svg'
  URL = 'http://static.bobdo.net/Analog_Clock.svg'


class PirateMarkPage(rendering_story.RenderingStory):
  BASE_NAME = 'ie_pirate_mark'
  URL = ('http://web.archive.org/web/20150502135732/'
         'http://ie.microsoft.com/testdrive/Performance/'
         'Pirates/Default.html')

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(PirateMarkPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForNetworkQuiescence()
    with action_runner.CreateInteraction('Filter'):
      action_runner.EvaluateJavaScript(
          'document.getElementById("benchmarkButtonText").click()')
      action_runner.Wait(10)

# TODO(crbug.com/760553):remove this class after smoothness.tough_filters_cases
# benchmark is completely replaced by rendering benchmarks
class ToughFiltersCasesPageSet(story.StorySet):

  """
  Description: Self-driven filters animation examples
  """

  def __init__(self):
    super(ToughFiltersCasesPageSet, self).__init__(
      archive_data_file='../data/tough_filters_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    page_classes = [
        MotionMarkPage,
        FilterTerrainSVGPage,
        AnalogClockSVGPage,
        PirateMarkPage
    ]

    for page_class in page_classes:
      self.AddStory(page_class(self))
