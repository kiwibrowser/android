# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story

from page_sets import webgl_supported_shared_state
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughWebglPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.REQUIRED_WEBGL, story_tags.TOUGH_WEBGL]

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    super(ToughWebglPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        make_javascript_deterministic=False)

  @property
  def skipped_gpus(self):
    # crbug.com/462729
    return ['arm', 'broadcom', 'hisilicon', 'imagination', 'qualcomm',
            'vivante']

  def RunNavigateSteps(self, action_runner):
    super(ToughWebglPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')
    action_runner.Wait(2)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('WebGLAnimation'):
      action_runner.Wait(5)


class NvidiaVertexBufferObjectPage(ToughWebglPage):
  BASE_NAME = 'nvidia_vertex_buffer_object'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/google/nvidia-vertex-buffer-object/index.html'


class SansAngelesPage(ToughWebglPage):
  BASE_NAME = 'san_angeles'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/google/san-angeles/index.html'


class ParticlesPage(ToughWebglPage):
  BASE_NAME = 'particles'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/google/particles/index.html'


class EarthPage(ToughWebglPage):
  BASE_NAME = 'earth'
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/webkit/Earth.html'


class ManyPlanetsDeepPage(ToughWebglPage):
  BASE_NAME = 'many_planets_deep'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/webkit/ManyPlanetsDeep.html'


class AquariumPage(ToughWebglPage):
  BASE_NAME = 'aquarium'
  URL = 'http://webglsamples.org/aquarium/aquarium.html'


class Aquarium20KFishPage(ToughWebglPage):
  BASE_NAME = 'aquarium_20k'
  URL = 'http://webglsamples.org/aquarium/aquarium.html?numFish=20000'


class BlobPage(ToughWebglPage):
  BASE_NAME = 'blob'
  URL = 'http://webglsamples.org/blob/blob.html'


class DynamicCubeMapPage(ToughWebglPage):
  BASE_NAME = 'dynamic_cube_map'
  URL = 'http://webglsamples.org/dynamic-cubemap/dynamic-cubemap.html'


class KenRussellPage(ToughWebglPage):
  BASE_NAME = 'animometer_webgl'
  # pylint: disable=line-too-long
  URL = 'http://kenrussell.github.io/webgl-animometer/Animometer/tests/3d/webgl.html'


# TODO(crbug.com/760553):remove this class after smoothness.tough_webgl_cases
# benchmark is completely replaced by rendering benchmarks
class ToughWebglCasesPageSet(story.StorySet):

  """
  Description: Self-driven WebGL animation examples
  """

  def __init__(self):
    super(ToughWebglCasesPageSet, self).__init__(
      archive_data_file='../data/tough_webgl_cases.json',
      cloud_storage_bucket=story.PUBLIC_BUCKET)

    page_classes = [
      NvidiaVertexBufferObjectPage,
      SansAngelesPage,
      ParticlesPage,
      EarthPage,
      ManyPlanetsDeepPage,
      AquariumPage,
      Aquarium20KFishPage,
      BlobPage,
      DynamicCubeMapPage,
      KenRussellPage
    ]
    for page_class in page_classes:
      self.AddStory(page_class(
          self,
          shared_page_state_class=(
            webgl_supported_shared_state.WebGLSupportedSharedState)))
