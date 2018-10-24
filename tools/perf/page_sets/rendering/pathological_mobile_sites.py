# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class PathologicalMobileSitesPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.PATHOLOGICAL_MOBILE_SITES]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(PathologicalMobileSitesPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class CnnPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'cnn_pathological'
  URL = 'http://edition.cnn.com'


class EspnPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'espn_pathological'
  URL = 'http://m.espn.go.com/nhl/rankings'


class RecodePathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'recode_pathological'
  URL = 'http://recode.net'


class YahooSportsPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'yahoo_sports_pathological'
  URL = 'http://sports.yahoo.com/'


class LaTimesPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'latimes_pathological'
  URL = 'http://www.latimes.com'


class PbsPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'pbs_pathological'
  # pylint: disable=line-too-long
  URL = 'http://www.pbs.org/newshour/bb/much-really-cost-live-city-like-seattle/#the-rundown'


class GuardianPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'guardian_pathological'
  # pylint: disable=line-too-long
  URL = 'http://www.theguardian.com/politics/2015/mar/09/ed-balls-tory-spending-plans-nhs-charging'


class ZDNetPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'zdnet_pathological'
  URL = 'http://www.zdnet.com'


class WowWikkiPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'wow_wiki_pathological'
  URL = 'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria'


class LinkedInPathologicalPage(PathologicalMobileSitesPage):
  BASE_NAME = 'linkedin_pathological'
  URL = 'https://www.linkedin.com/in/linustorvalds'


# TODO(crbug.com/760553):remove this class after
# smoothness.pathological_mobile_sites benchmark is completely
# replaced by rendering benchmarks
class PathologicalMobileSitesPageSet(story.StorySet):

  """Pathologically bad and janky sites on mobile."""

  def __init__(self):
    super(PathologicalMobileSitesPageSet, self).__init__(
        archive_data_file='../data/pathological_mobile_sites.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    page_classes = [CnnPathologicalPage,
                    EspnPathologicalPage,
                    RecodePathologicalPage,
                    YahooSportsPathologicalPage,
                    LaTimesPathologicalPage,
                    PbsPathologicalPage,
                    GuardianPathologicalPage,
                    ZDNetPathologicalPage,
                    WowWikkiPathologicalPage,
                    LinkedInPathologicalPage]

    for page_class in page_classes:
      self.AddStory(page_class(self))
