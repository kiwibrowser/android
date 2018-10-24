#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys
import tempfile


QUERY_BY_BUILD_NUMBER = """
SELECT
  run.name AS name,
  SUM(run.times) AS duration
FROM
  [test-results-hrd:events.test_results]
WHERE
  buildbot_info.builder_name IN ({})
  AND buildbot_info.build_number = {}
GROUP BY
  name
ORDER BY
  name
"""


QUERY_LAST_100_RUNS = """
SELECT
  name,
  AVG(run_durations) AS duration
FROM (
  SELECT
    name,
    start_time,
    SUM(run.times) AS run_durations,
    ROW_NUMBER() OVER (PARTITION BY name ORDER BY start_time DESC)
      AS row_num
  FROM (
    SELECT
      run.name AS name,
      start_time,
      run.times
    FROM
      [test-results-hrd:events.test_results]
    WHERE
      buildbot_info.builder_name IN ({})
      AND run.time IS NOT NULL
      AND run.time != 0
      AND run.is_unexpected IS FALSE
    GROUP BY
      name,
      start_time,
      run.times
    ORDER BY
      start_time DESC )
  GROUP BY
    name,
    start_time)
WHERE
  row_num < 100
GROUP BY
  name
ORDER BY
  name
"""


def _run_query(query, fd):
  subprocess.check_call(
      ["bq", "query", "--format=json", "--max_rows=100000", query],
      stdout=fd)


def main(args):
  """
    To run this script, you need to be able to run bigquery in your terminal.

    1) Follow the steps at https://cloud.google.com/sdk/docs/ to download and
       unpack google-cloud-sdk in your home directory.
    2) Run 'bq help' in your terminal
    3) Select 'test-results-hrd' as the project
       3a) If 'test-results-hrd' does not show up, contact seanmccullough@
           to be added as a user of the table
    4) Run this script!
  """
  parser = argparse.ArgumentParser(
      description='Retrieve story timing from bigquery.')
  parser.add_argument(
      '--output-file', action='store', required=True,
      help='The filename to send the bigquery results to.')
  parser.add_argument(
      '--configurations', '-c', action='append', required=True,
      help='The configuration(s) of the builder to query results from.')
  parser.add_argument(
      '--build-number', action='store',
      help='If specified, the build number to get timing data from.')
  opts = parser.parse_args(args)

  fd, path = tempfile.mkstemp(suffix='.json')
  try:
    configurations = str(opts.configurations).strip('[]')
    if opts.build_number:
      _run_query(QUERY_BY_BUILD_NUMBER.format(
          configurations, opts.build_number), fd)
    else:
      _run_query(QUERY_LAST_100_RUNS.format(configurations), fd)
    with os.fdopen(fd, 'r') as f:
      f.seek(0)
      timing_data = json.loads(f.read())
    with open(opts.output_file, 'w') as output_file:
      json.dump(timing_data, output_file, indent = 4, separators=(',', ': '))
  finally:
    os.remove(path)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
