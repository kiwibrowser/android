# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.login_helpers import google_login
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class TopRealWorldDesktopPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOP_REAL_WORLD_DESKTOP]

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    super(TopRealWorldDesktopPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
      action_runner.Wait(1)
      with action_runner.CreateGestureInteraction('ScrollAction'):
        action_runner.ScrollPage()
        if self.story_set.scroll_forever:
          while True:
            action_runner.ScrollPage(direction='up')
            action_runner.ScrollPage(direction='down')


class GoogleWebSearchPage(TopRealWorldDesktopPage):
  """ Why: top google property; a google tab is often open """
  BASE_NAME = 'google_web_search'
  URL = 'https://www.google.com/#hl=en&q=barack+obama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleWebSearchPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(GoogleWebSearchPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class GoogleImageSearchPage(TopRealWorldDesktopPage):
  """ Why: tough image case; top google properties """
  BASE_NAME = 'google_image_search'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleImageSearchPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GoogleImageSearchPage, self).RunNavigateSteps(action_runner)


class GmailPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  ABSTRACT_STORY = True
  URL = 'https://mail.google.com/mail/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GmailPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GmailPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')


class GoogleCalendarPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  ABSTRACT_STORY = True
  URL='https://www.google.com/calendar/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleCalendarPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GoogleCalendarPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
    action_runner.WaitForElement('div[class~="navForward"]')
    action_runner.ExecuteJavaScript("""
        (function() {
          var elem = document.createElement('meta');
          elem.name='viewport';
          elem.content='initial-scale=1';
          document.body.appendChild(elem);
        })();""")
    action_runner.Wait(1)


class GoogleDocPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties; Sample doc in the link """
  ABSTRACT_STORY = True
  # pylint: disable=line-too-long
  URL = 'https://docs.google.com/document/d/1X-IKNjtEnx-WW5JIKRLsyhz5sbsat3mfTpAPUSX3_s4/view'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleDocPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GoogleDocPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("kix-appview-editor").length')


class GooglePlusPage(TopRealWorldDesktopPage):
  """ Why: social; top google property; Public profile; infinite scrolls """
  BASE_NAME = 'google_plus'
  URL = 'https://plus.google.com/110031535020051778989/posts'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GooglePlusPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(GooglePlusPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Home')


class YoutubePage(TopRealWorldDesktopPage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube'
  URL = 'http://www.youtube.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(YoutubePage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')
    super(YoutubePage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class BlogspotPage(TopRealWorldDesktopPage):
  """ Why: #11 (Alexa global), google property; some blogger layouts have
  infinite scroll but more interesting """
  BASE_NAME = 'blogspot'
  URL = 'http://googlewebmastercentral.blogspot.com/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(BlogspotPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(BlogspotPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='accessibility')


class WordpressPage(TopRealWorldDesktopPage):
  """ Why: #18 (Alexa global), Picked an interesting post """
  BASE_NAME = 'wordpress'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(WordpressPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(WordpressPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(
        # pylint: disable=line-too-long
        'a[href="http://en.blog.wordpress.com/2012/08/30/new-themes-able-and-sight/"]'
    )


class FacebookPage(TopRealWorldDesktopPage):
  """ Why: top social,Public profile """
  BASE_NAME = 'facebook'
  URL = 'https://www.facebook.com/barackobama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(FacebookPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(FacebookPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Videos')


class LinkedinPage(TopRealWorldDesktopPage):
  """ Why: #12 (Alexa global), Public profile. """
  BASE_NAME = 'linkedin'
  URL = 'http://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(LinkedinPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class WikipediaPage(TopRealWorldDesktopPage):
  """ Why: #6 (Alexa) most visited worldwide,Picked an interesting page. """
  BASE_NAME = 'wikipedia'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(WikipediaPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class TwitterPage(TopRealWorldDesktopPage):
  """ Why: #8 (Alexa global),Picked an interesting page """
  BASE_NAME = 'twitter'
  URL = 'https://twitter.com/katyperry'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(TwitterPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(TwitterPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class PinterestPage(TopRealWorldDesktopPage):
  """ Why: #37 (Alexa global) """
  BASE_NAME = 'pinterest'
  URL = 'http://pinterest.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(PinterestPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class ESPNPage(TopRealWorldDesktopPage):
  """ Why: #1 sports """
  ABSTRACT_STORY = True
  URL = 'http://espn.go.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(ESPNPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class WeatherPage(TopRealWorldDesktopPage):
  """ Why: #7 (Alexa news); #27 total time spent, picked interesting page. """
  BASE_NAME = 'weather.com'
  URL = 'http://www.weather.com/weather/right-now/Mountain+View+CA+94043'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(WeatherPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class YahooGamesPage(TopRealWorldDesktopPage):
  """ Why: #1 games according to Alexa (with actual games in it) """
  BASE_NAME = 'yahoo_games'
  URL = 'http://games.yahoo.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(YahooGamesPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(YahooGamesPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)


class GmailSmoothPage(GmailPage):
  """ Why: productivity, top google properties """
  BASE_NAME = 'gmail'

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript("""
        gmonkey.load('2.0', function(api) {
          window.__scrollableElementForTelemetry = api.getScrollableElement();
        });""")
    action_runner.WaitForJavaScriptCondition(
        'window.__scrollableElementForTelemetry != null')
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          element_function='window.__scrollableElementForTelemetry')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up',
              element_function='window.__scrollableElementForTelemetry')
          action_runner.ScrollElement(
              direction='down',
              element_function='window.__scrollableElementForTelemetry')


class GoogleCalendarSmoothPage(GoogleCalendarPage):
  """ Why: productivity, top google properties """
  BASE_NAME='google_calendar'

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#scrolltimedeventswk')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='#scrolltimedeventswk')
          action_runner.ScrollElement(
              direction='down', selector='#scrolltimedeventswk')


class GoogleDocSmoothPage(GoogleDocPage):
  """ Why: productivity, top google properties; Sample doc in the link """
  BASE_NAME='google_docs'

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='.kix-appview-editor')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='.kix-appview-editor')
          action_runner.ScrollElement(
              direction='down', selector='.kix-appview-editor')


class ESPNSmoothPage(ESPNPage):
  """ Why: #1 sports """
  BASE_NAME='espn'

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(left_start_ratio=0.1)
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up', left_start_ratio=0.1)
          action_runner.ScrollPage(direction='down', left_start_ratio=0.1)


class YahooNewsPage(TopRealWorldDesktopPage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news'
  URL = 'http://news.yahoo.com'


class CNNNewsPage(TopRealWorldDesktopPage):
  """Why: #2 news worldwide"""
  BASE_NAME = 'cnn'
  URL = 'http://www.cnn.com'


class AmazonPage(TopRealWorldDesktopPage):
  # Why: #1 world commerce website by visits; #3 commerce in the US by
  # time spent
  BASE_NAME = 'amazon'
  URL = 'http://www.amazon.com'


class EbayPage(TopRealWorldDesktopPage):
  # Why: #1 commerce website by time spent by users in US
  BASE_NAME = 'ebay'
  URL = 'http://www.ebay.com'


class BookingPage(TopRealWorldDesktopPage):
  # Why: #1 Alexa recreation
  BASE_NAME = 'booking.com'
  URL = 'http://booking.com'


class YahooAnswersPage(TopRealWorldDesktopPage):
  # Why: #1 Alexa reference
  BASE_NAME = 'yahoo_answers'
  URL = 'http://answers.yahoo.com'


class YahooSportsPage(TopRealWorldDesktopPage):
  # Why: #1 Alexa sports
  BASE_NAME = 'yahoo_sports'
  URL = 'http://sports.yahoo.com/'


class TechCrunchPage(TopRealWorldDesktopPage):
  # Why: top tech blog
  BASE_NAME = 'techcrunch'
  URL = 'http://techcrunch.com'
