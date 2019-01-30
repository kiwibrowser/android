# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

from typing import Any, Dict, List, Tuple, Optional


def UnitStringIsValid(unit: str) -> bool:
  return (unit == "us/hop" or unit == "us/task" or unit == "ns/sample" or
          unit == "ms" or unit == "s" or unit == "count")


class ResultLine(object):
  """ResultLine objects are each an individual line of output, complete with a
  unit, measurement, and descriptive component
  """

  def __init__(self, desc: str, measure: float, unit: str) -> None:
    self.desc = desc
    self.meas = measure
    self.unit = unit

  def ToJsonDict(self) -> Dict[str, Any]:
    return {
        "description": self.desc,
        "measurement": self.meas,
        "unit": self.unit,
    }


def ResultLineFromStdout(line: str) -> Optional[ResultLine]:
  if "pkgsvr" in line:
    return None
  chunks = line.split()
  # There should be 1 chunk for the measure, 1 for the unit, and at least one
  # for the line description, so at least 3 total
  if len(chunks) < 3:
    logging.warning("The line {} contains too few space-separated pieces to be "
        "parsed as a ResultLine".format(line))
    return None
  unit = chunks[-1]
  if not UnitStringIsValid(unit):
    logging.warning("The unit string parsed from {} was {}, which was not "
        "expected".format(line, unit))
    return None
  try:
    measure = float(chunks[-2])
    desc = " ".join(chunks[:-2])
    return ResultLine(desc, measure, unit)
  except ValueError as e:
    logging.warning("The chunk {} could not be parsed as a valid measurement "
        "because of {}".format(chunks[-2], str(e)))
    return None


class TestResult(object):
  """TestResult objects comprise the smallest unit of a GTest target, and
  contain the name of the individual test run, and the time that the test took
  to run."""

  def __init__(self, name: str, time: float, lines: List[ResultLine]) -> None:
    self.name = name
    self.time = time
    self.lines = lines

  def ToJsonDict(self) -> Dict[str, Any]:
    return {
        "name": self.name,
        "time_in_ms": self.time,
        "lines": [line.ToJsonDict() for line in self.lines]
    }


def ExtractCaseInfo(line: str) -> Tuple[str, float, str]:
  # Trim off the [       OK ] part of the line
  trimmed = line.lstrip("[       OK ]").strip()
  try:
    test_name, rest = trimmed.split("(")  # Isolate the measurement
  except Exception as e:
    err_text = "Could not extract the case name from {} because of error {}"\
               .format(
        rest, str(e))
    raise Exception(err_text)
  try:
    measure, units = rest.split(")")[0].split()
  except Exception as e:
    err_text = "Could not extract measure and units from {}\
                because of error {}".format(rest, str(e))
    raise Exception(err_text)
  return test_name.strip(), float(measure), units.strip()


def TaggedTestFromLines(lines: List[str]) -> TestResult:
  test_name, time, units = ExtractCaseInfo(lines[-1])
  res_lines = []
  for line in lines[:-1]:
    res_line = ResultLineFromStdout(line)
    if res_line:
      res_lines.append(res_line)
    else:
      logging.warning("Couldn't parse line {} into a ResultLine".format(line))
  return TestResult(test_name, time, res_lines)


class TargetResult(object):
  """TargetResult objects contain the entire set of TestSuite objects that are
  invoked by a single test target, such as base:base_perftests and the like.
  They also include the name of the target, and the time it took the target to
  run.
  """

  def __init__(self, name: str, time: float, tests: List[TestResult]) -> None:
    self.name = name
    self.time = time
    self.tests = tests

  def ToJsonDict(self) -> Dict[str, Any]:
    return {
        "name": self.name,
        "time_in_ms": self.time,
        "tests": [test.ToJsonDict() for test in self.tests]
    }

  def WriteToJson(self, path: str) -> None:
    with open(path, "w") as outfile:
      json.dump(self.ToJsonDict(), outfile, indent=2)


def TargetResultFromStdout(lines: List[str], name: str) -> TargetResult:
  """TargetResultFromStdout attempts to associate GTest names to the lines of
  output that they produce. Example input looks something like the following:

  [ RUN      ] TestNameFoo
  INFO measurement units
  ...
  [       OK ] TestNameFoo (measurement units)
  ...

  Unfortunately, Because the results of output from perftest targets is not
  necessarily consistent between test targets, this makes a best-effort to parse
  as much information from them as possible.
  """
  test_line_lists = []  # type: List[List[str]]
  test_line_accum = []  # type: List[str]
  read_lines = False
  for line in lines:
    # We're starting a test suite
    if line.startswith("[ RUN      ]"):
      read_lines = True
      # We have a prior suite that needs to be added
      if len(test_line_accum) > 0:
        test_line_lists.append(test_line_accum)
        test_line_accum = []
    elif read_lines:
      # We don't actually care about the data in the RUN line, just its
      # presence. the OK line contains the same info, as well as the total test
      # run time
      test_line_accum.append(line)
      if line.startswith("[       OK ]"):
        read_lines = False

  test_cases = [
      TaggedTestFromLines(test_lines) for test_lines in test_line_lists
  ]
  target_time = 0  # type: float
  for test in test_cases:
    target_time += test.time
  return TargetResult(name, target_time, test_cases)
