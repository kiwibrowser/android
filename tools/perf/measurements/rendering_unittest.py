# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.testing import options_for_unittests
from telemetry.testing import page_test_test_case
from telemetry.util import wpr_modes

from measurements import rendering

RENDERING_THREAD_GROUPS = ['all', 'browser', 'fast_path', 'gpu', 'io', 'other',
                           'raster', 'renderer_compositor', 'renderer_main']

THREAD_TIMES_THREAD_GROUPS = ['GPU', 'IO', 'browser', 'display_compositor',
                              'other', 'raster', 'renderer_compositor',
                              'renderer_main', 'total_all', 'total_fast_path']

class RenderingUnitTest(page_test_test_case.PageTestTestCase):
  """Test for rendering measurement

     Runs rendering measurement on a simple page and verifies the existence of
     all metrics in the results. The correctness of metrics is not tested here.
  """

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF
    self._options.pageset_repeat = 2

  def testRendering(self):
    ps = self.CreateStorySetFromFileInUnittestDataDir('scrollable_page.html')
    results = self.RunMeasurement(
        rendering.Rendering(), ps, options=self._options)
    self.assertFalse(results.had_failures)

    # Build a map from histogram name to the number of sample values.
    num_samples = {}
    for histogram in results.AsHistogramDicts():
      if 'name' in histogram and 'sampleValues' in histogram:
        histogram_name = histogram['name']
        current_num_samples = len(histogram['sampleValues'])
        if histogram_name in num_samples:
          num_samples[histogram_name] += current_num_samples
        else:
          num_samples[histogram_name] = current_num_samples

    # Check the existence of cores_per_second metrics.
    for thread_group in RENDERING_THREAD_GROUPS:
      # We should have at least two sample values for each metric, since
      # pageset_repeat is 2.
      self.assertGreater(
          num_samples.get('cores_per_second_%s_thread' % thread_group, 0), 1)

    # Check the existence of some of the legacy metrics.
    self.assertGreater(num_samples.get('frame_times', 0), 1)
    self.assertGreater(num_samples.get('percentage_smooth', 0), 1)
    for thread_group in THREAD_TIMES_THREAD_GROUPS:
      self.assertGreater(
          num_samples.get('thread_%s_cpu_time_per_second' % thread_group, 0), 1)
      self.assertGreater(
          num_samples.get('thread_%s_cpu_time_per_frame' % thread_group, 0), 1)
