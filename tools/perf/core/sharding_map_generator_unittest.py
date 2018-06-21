# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import copy
from core import sharding_map_generator
import json
import os
import tempfile
import unittest


class TestShardingMapGenerator(unittest.TestCase):

  def _init_sample_timing_data(self, times):
    timing_data = OrderedDict()
    all_stories = {}
    for i in range(len(times)):
      all_stories['benchmark_' + str(i)] = []
      story_times = times[i]
      for j in range(len(story_times)):
        all_stories['benchmark_' + str(i)].append('story_' + str(j))
        timing_data['benchmark_' + str(i) + '/' + 'story_' + str(j)] = (
            story_times[j])
    return timing_data, all_stories

  def testGenerateAndTestShardingMap(self):
    timing_data, all_stories = self._init_sample_timing_data(
        [[60, 56, 57], [66, 54, 80, 4], [2, 8, 7, 37, 2]])
    timing_data_for_testing = copy.deepcopy(timing_data)
    sharding_map = sharding_map_generator.generate_sharding_map(
        timing_data, all_stories, 3, None)
    fd_map, map_path = tempfile.mkstemp(suffix='.json')
    try:
      with os.fdopen(fd_map, 'w') as f:
        json.dump(sharding_map, f)
      results = sharding_map_generator.test_sharding_map(map_path,
          timing_data_for_testing, all_stories)
      self.assertEqual(results['0']['full_time'], 116)
      self.assertEqual(results['1']['full_time'], 177)
      self.assertEqual(results['2']['full_time'], 140)
    finally:
      os.remove(map_path)
