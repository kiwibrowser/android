# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class TopRealWorldMobilePage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.SYNC_SCROLL, story_tags.TOP_REAL_WORLD_MOBILE]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TopRealWorldMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class CapitolVolkswagenMobilePage(TopRealWorldMobilePage):
  """ Why: Typical mobile business site """
  BASE_NAME = 'capitolvolkswagen_mobile'
  URL = ('http://iphone.capitolvolkswagen.com/index.htm'
         '#new-inventory_p_2Fsb-new_p_2Ehtm_p_3Freset_p_3DInventoryListing')

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CapitolVolkswagenMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CapitolVolkswagenMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next 35')
    action_runner.WaitForJavaScriptCondition(
        'document.body.scrollHeight > 2560')


class TheVergeArticleMobilePage(TopRealWorldMobilePage):
  """ Why: Top tech blog """
  BASE_NAME = 'theverge_article_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.theverge.com/2012/10/28/3568746/amazon-7-inch-fire-hd-ipad-mini-ad-ballsy'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TheVergeArticleMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(TheVergeArticleMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.Chorus !== undefined &&'
        'window.Chorus.Comments !== undefined &&'
        'window.Chorus.Comments.Json !== undefined &&'
        '(window.Chorus.Comments.loaded ||'
        ' window.Chorus.Comments.Json.load_comments())')


class CnnArticleMobilePage(TopRealWorldMobilePage):
  """ Why: Top news site """
  BASE_NAME = 'cnn_article_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.cnn.com/2012/10/03/politics/michelle-obama-debate/index.html'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CnnArticleMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CnnArticleMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(8)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      # With default top_start_ratio=0.5 the corresponding element in this page
      # will not be in the root scroller.
      action_runner.ScrollPage(top_start_ratio=0.01)


class FacebookMobilePage(TopRealWorldMobilePage):
  """ Why: #1 (Alexa global) """
  BASE_NAME = 'facebook_mobile'
  URL = 'https://facebook.com/barackobama'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FacebookMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(FacebookMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("u_0_c") !== null &&'
        'document.body.scrollHeight > window.innerHeight')


class YoutubeMobilePage(TopRealWorldMobilePage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube_mobile'
  URL = 'http://m.youtube.com/watch?v=9hBpF_Zj4OA'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YoutubeMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YoutubeMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("paginatortarget") !== null')


class LinkedInMobilePage(TopRealWorldMobilePage):
  """ Why: #12 (Alexa global),Public profile """
  BASE_NAME = 'linkedin_mobile'
  URL = 'https://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(LinkedInMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Linkedin has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(LinkedInMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-view-scroller") !== null')

    action_runner.ScrollPage()

    super(LinkedInMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-view-scroller") !== null')


class YahooAnswersMobilePage(TopRealWorldMobilePage):
  """ Why: #1 Alexa reference """
  BASE_NAME = 'yahoo_answers_mobile'
  # pylint: disable=line-too-long
  URL = 'http://answers.yahoo.com/question/index?qid=20110117024343AAopj8f'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YahooAnswersMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YahooAnswersMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Other Answers (1 - 20 of 149)')
    action_runner.ClickElement(text='Other Answers (1 - 20 of 149)')


class GoogleNewsMobilePage(TopRealWorldMobilePage):
  """ Why: Google News: accelerated scrolling version """
  BASE_NAME = 'google_news_mobile'
  URL = 'http://mobile-news.sandbox.google.com/news/pt1'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(GoogleNewsMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(GoogleNewsMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'typeof NEWS_telemetryReady !== "undefined" && '
        'NEWS_telemetryReady == true')


class AmazonNicolasCageMobilePage(TopRealWorldMobilePage):
  """
  Why: #1 world commerce website by visits; #3 commerce in the US by time spent
  """
  BASE_NAME = 'amazon_mobile'
  URL = 'http://www.amazon.com/gp/aw/s/ref=is_box_?k=nicolas+cage'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(AmazonNicolasCageMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          selector='#search',
          distance_expr='document.body.scrollHeight - window.innerHeight')


class WowwikiMobilePage(TopRealWorldMobilePage):
  """Why: Mobile wiki."""
  BASE_NAME = 'wowwiki_mobile'
  URL = 'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WowwikiMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Wowwiki has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(WowwikiMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.ScrollPage()
    super(WowwikiMobilePage, self).RunNavigateSteps(action_runner)


class WikipediaDelayedScrollMobilePage(TopRealWorldMobilePage):
  """Why: Wikipedia page with a delayed scroll start"""
  BASE_NAME = 'wikipedia_delayed_scroll_start'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WikipediaDelayedScrollMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
      'document.readyState == "complete"', timeout=30)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class BlogspotMobilePage(TopRealWorldMobilePage):
  """Why: #11 (Alexa global), google property"""
  BASE_NAME = 'blogspot_mobile'
  URL = 'http://googlewebmastercentral.blogspot.com/'


class WordpressMobilePage(TopRealWorldMobilePage):
  """Why: #18 (Alexa global), Picked an interesting post"""
  BASE_NAME = 'wordpress_mobile'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'


class WikipediaMobilePage(TopRealWorldMobilePage):
  """Why: #6 (Alexa) most visited worldwide, picked an interesting page"""
  BASE_NAME = 'wikipedia_mobile'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'


class TwitterMobilePage(TopRealWorldMobilePage):
  """Why: #8 (Alexa global), picked an interesting page"""
  BASE_NAME = 'twitter_mobile'
  URL = 'http://twitter.com/katyperry'


class PinterestMobilePage(TopRealWorldMobilePage):
  """Why: #37 (Alexa global)."""
  BASE_NAME = 'pinterest_mobile'
  URL = 'http://pinterest.com'


class ESPNMobilePage(TopRealWorldMobilePage):
  """Why: #1 sports."""
  BASE_NAME = 'espn_mobile'
  URL = 'http://espn.go.com'


class ForecastIOMobilePage(TopRealWorldMobilePage):
  """Why: crbug.com/231413"""
  BASE_NAME = 'forecast.io_mobile'
  URL = 'http://forecast.io'


class GooglePlusMobilePage(TopRealWorldMobilePage):
  """Why: Social; top Google property; Public profile; infinite scrolls."""
  BASE_NAME = 'google_plus_mobile'
  # pylint: disable=line-too-long
  URL = 'https://plus.google.com/app/basic/110031535020051778989/posts?source=apppromo'


class AndroidPoliceMobilePage(TopRealWorldMobilePage):
  """Why: crbug.com/242544"""
  BASE_NAME = 'androidpolice_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.androidpolice.com/2012/10/03/rumor-evidence-mounts-that-an-lg-optimus-g-nexus-is-coming-along-with-a-nexus-phone-certification-program/'


class GSPMobilePage(TopRealWorldMobilePage):
  """Why: crbug.com/149958"""
  BASE_NAME = 'gsp.ro_mobile'
  URL = 'http://gsp.ro'


class TheVergeMobilePage(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'theverge_mobile'
  URL = 'http://theverge.com'


class DiggMobilePage(TopRealWorldMobilePage):
  """Why: Top tech site"""
  BASE_NAME = 'digg_mobile'
  URL = 'http://digg.com'


class GoogleSearchMobilePage(TopRealWorldMobilePage):
  """Why: Top Google property; a Google tab is often open"""
  BASE_NAME = 'google_web_search_mobile'
  URL = 'https://www.google.co.uk/search?hl=en&q=barack+obama&cad=h'


class YahooNewsMobilePage(TopRealWorldMobilePage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news_mobile'
  URL = 'http://news.yahoo.com'


class CnnNewsMobilePage(TopRealWorldMobilePage):
  """# Why: #2 news worldwide"""
  BASE_NAME = 'cnn_mobile'
  URL = 'http://www.cnn.com'


class EbayMobilePage(TopRealWorldMobilePage):
  """Why: #1 commerce website by time spent by users in US"""
  BASE_NAME = 'ebay_mobile'
  URL = 'http://shop.mobileweb.ebay.com/searchresults?kw=viking+helmet'


class BookingMobilePage(TopRealWorldMobilePage):
  """Why: #1 Alexa recreation"""
  BASE_NAME = 'booking.com_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.booking.com/searchresults.html?src=searchresults&latitude=65.0500&longitude=25.4667'


class TechCrunchMobilePage(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'techcrunch_mobile'
  URL = 'http://techcrunch.com'


class MLBMobilePage(TopRealWorldMobilePage):
  """Why: #6 Alexa sports"""
  BASE_NAME = 'mlb_mobile'
  URL = 'http://mlb.com/'


class SFGateMobilePage(TopRealWorldMobilePage):
  """Why: #14 Alexa California"""
  BASE_NAME = 'sfgate_mobile'
  URL = 'http://www.sfgate.com/'


class WorldJournalMobilePage(TopRealWorldMobilePage):
  """Why: Non-latin character set"""
  BASE_NAME = 'worldjournal_mobile'
  URL = 'http://worldjournal.com/'


class WSJMobilePage(TopRealWorldMobilePage):
  """Why: #15 Alexa news"""
  BASE_NAME = 'wsj_mobile'
  URL = 'http://online.wsj.com/home-page'


class DeviantArtMobilePage(TopRealWorldMobilePage):
  """Why: Image-heavy mobile site"""
  BASE_NAME = 'deviantart_mobile'
  URL = 'http://www.deviantart.com/'


class BaiduMobilePage(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'baidu_mobile'
  # pylint: disable=line-too-long
  URL = 'http://www.baidu.com/s?wd=barack+obama&rsv_bp=0&rsv_spt=3&rsv_sug3=9&rsv_sug=0&rsv_sug4=3824&rsv_sug1=3&inputT=4920'


class BingMobilePage(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'bing_mobile'
  URL = 'http://www.bing.com/search?q=sloths'


class USATodayMobilePage(TopRealWorldMobilePage):
  """Why: Good example of poor initial scrolling"""
  BASE_NAME = 'usatoday_mobile'
  URL = 'http://ftw.usatoday.com/2014/05/spelling-bee-rules-shenanigans'


class FastPathSmoothMobilePage(TopRealWorldMobilePage):
  ABSTRACT_STORY = True
  TAGS = [story_tags.FASTPATH]

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FastPathSmoothMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class NYTimesMobilePage(FastPathSmoothMobilePage):
  """Why: Top news site."""
  BASE_NAME = 'nytimes_mobile'
  URL = 'http://nytimes.com/'


class CuteOverloadMobilePage(FastPathSmoothMobilePage):
  """Why: Image-heavy site."""
  BASE_NAME = 'cuteoverload_mobile'
  URL = 'http://cuteoverload.com'


class RedditMobilePage(FastPathSmoothMobilePage):
  """Why: #5 Alexa news."""
  BASE_NAME = 'reddit_mobile'
  URL = 'http://www.reddit.com/r/programming/comments/1g96ve'


class BoingBoingMobilePage(FastPathSmoothMobilePage):
  """Why: Problematic use of fixed position elements."""
  BASE_NAME = 'boingboing_mobile'
  URL = 'http://www.boingboing.net'


class SlashDotMobilePage(FastPathSmoothMobilePage):
  """Why: crbug.com/169827"""
  BASE_NAME = 'slashdot_mobile'
  URL = 'http://slashdot.org'
