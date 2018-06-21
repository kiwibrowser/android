# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughPathRenderingPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_PATH_RENDERING]

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ClickStart'):
      action_runner.Wait(10)


class GUIMarkVectorChartPage(ToughPathRenderingPage):
  BASE_NAME = 'guimark_vector_chart'
  URL = 'http://www.craftymind.com/factory/guimark2/HTML5ChartingTest.html'


class MotionMarkCanvasFillShapesPage(ToughPathRenderingPage):
  BASE_NAME = 'motion_mark_canvas_fill_shapes'
  # pylint: disable=line-too-long
  URL = 'http://rawgit.com/WebKit/webkit/master/PerformanceTests/MotionMark/developer.html?test-name=Fillshapes&test-interval=20&display=minimal&tiles=big&controller=fixed&frame-rate=50&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Canvassuite&complexity=1000'


class MotionMarkCanvasStrokeShapesPage(ToughPathRenderingPage):
  BASE_NAME = 'motion_mark_canvas_stroke_shapes'
  # pylint: disable=line-too-long
  URL = 'http://rawgit.com/WebKit/webkit/master/PerformanceTests/MotionMark/developer.html?test-name=Strokeshapes&test-interval=20&display=minimal&tiles=big&controller=fixed&frame-rate=50&kalman-process-error=1&kalman-measurement-error=4&time-measurement=performance&suite-name=Canvassuite&complexity=1000'


class ChalkboardPage(rendering_story.RenderingStory):
  BASE_NAME = 'ie_chalkboard'
  URL = 'https://testdrive-archive.azurewebsites.net/performance/chalkboard/'
  TAGS = [story_tags.TOUGH_PATH_RENDERING]

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ClickStart'):
      action_runner.EvaluateJavaScript(
          'document.getElementById("StartButton").click()')
      action_runner.Wait(20)


# TODO(crbug.com/760553): remove this class after
# smoothness.tough_path_rendering_cases benchmark is completely replaced
# by rendering benchmarks
class ToughPathRenderingCasesPageSet(story.StorySet):

  """
  Description: Self-driven path rendering examples
  """

  def __init__(self):
    super(ToughPathRenderingCasesPageSet, self).__init__(
      archive_data_file='../data/tough_path_rendering_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    page_classes = [
      GUIMarkVectorChartPage,
      MotionMarkCanvasFillShapesPage,
      MotionMarkCanvasStrokeShapesPage,
      ChalkboardPage
    ]

    for page_class in page_classes:
      self.AddStory(page_class(
          page_set=self,
          shared_page_state_class=shared_page_state.SharedPageState))
