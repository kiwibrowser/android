#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script takes in a list of test targets to be run on both Linux and
# Fuchsia devices and then compares their output to each other, extracting the
# relevant performance data from the output of gtest.

import os
import re
import subprocess
import sys
import time

from collections import defaultdict
from typing import Tuple, Dict, List

import target_spec
import test_results


def RunCommand(command: List[str], msg: str) -> str:
  "One-shot start and complete command with useful default kwargs"
  command = [piece for piece in command if piece != ""]
  proc = subprocess.run(
      command,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      stdin=subprocess.DEVNULL)
  out = proc.stdout.decode("utf-8", errors="ignore")
  err = proc.stderr.decode("utf-8", errors="ignore")
  if proc.returncode != 0:
    sys.stderr.write("{}\nreturn code: {}\nstdout: {}\nstderr: {}".format(
        msg, proc.returncode, out, err))
    raise subprocess.SubprocessError(
        "Command failed to complete successfully. {}".format(command))
  return out


# TODO(crbug.com/848465): replace with --test-launcher-filter-file directly
def ParseFilterFile(filepath: str) -> str:
  positive_filters = []
  negative_filters = []
  with open(filepath, "r") as file:
    for line in file:
      # Only take the part of a line before a # sign
      line = line.split("#", 1)[0].strip()
      if line == "":
        continue
      elif line.startswith("-"):
        negative_filters.append(line[1:])
      else:
        positive_filters.append(line)

  return "--gtest_filter={}-{}".format(":".join(positive_filters),
                                       ":".join(negative_filters))


class TestTarget(object):
  """TestTarget encapsulates a single BUILD.gn target, extracts a name from the
  target string, and manages the building and running of the target for both
  Linux and Fuchsia.
  """

  def __init__(self, target: str) -> None:
    self._target = target
    self._name = target.split(":")[-1]
    self._filter_file = "testing/buildbot/filters/fuchsia.{}.filter".format(
        self._name)
    if not os.path.isfile(self._filter_file):
      self._filter_flag = ""
      self._filter_file = ""
    else:
      self._filter_flag = ParseFilterFile(self._filter_file)

  def ExecFuchsia(self, out_dir: str, run_locally: bool) -> str:
    runner_name = "{}/bin/run_{}".format(out_dir, self._name)
    command = [runner_name, self._filter_flag, "--include-system-logs=False"]
    if not run_locally:
      command.append("-d")
    command.append(self._filter_flag)
    return RunCommand(command,
                      "Test {} failed on fuchsia!".format(self._target))

  def ExecLinux(self, out_dir: str, run_locally: bool) -> str:
    command = []  # type: List[str]
    user = target_spec.linux_device_user
    ip = target_spec.linux_device_ip
    host_machine = "{0}@{1}".format(user, ip)
    if not run_locally:
      # Next is the transfer of all the directories to the destination device.
      self.TransferDependencies(out_dir, host_machine)
      command = [
          "ssh", "{}@{}".format(user, ip), "{1}/{0}/{1} -- {2}".format(
              out_dir, self._name, self._filter_flag)
      ]
    else:
      local_path = "{}/{}".format(out_dir, self._name)
      command = [local_path, "--", self._filter_flag]
    return RunCommand(command, "Test {} failed on linux!".format(self._target))

  def TransferDependencies(self, out_dir: str, host: str):
    gn_desc = ["gn", "desc", out_dir, self._target, "runtime_deps"]
    out = RunCommand(
        gn_desc, "Failed to get dependencies of target {}".format(self._target))

    paths = []
    for line in out.split("\n"):
      if line == "":
        continue
      line = out_dir + "/" + line.strip()
      line = os.path.abspath(line)
      paths.append(line)
    common = os.path.commonpath(paths)
    paths = [os.path.relpath(path, common) for path in paths]

    archive_name = self._name + ".tar.gz"
    # Compress the dependencies of the test.
    command = ["tar", "-czf", archive_name] + paths
    if self._filter_file != "":
      command.append(self._filter_file)
    RunCommand(
        command,
        "{} dependency compression failed".format(self._target),
    )
    # Make sure the containing directory exists on the host, for easy cleanup.
    RunCommand(["ssh", host, "mkdir -p {}".format(self._name)],
               "Failed to create directory on host for {}".format(self._target))
    # Transfer the test deps to the host.
    RunCommand(
        [
            "scp", archive_name, "{}:{}/{}".format(host, self._name,
                                                   archive_name)
        ],
        "{} dependency transfer failed".format(self._target),
    )
    # Decompress the dependencies once they're on the host.
    RunCommand(
        [
            "ssh", host, "tar -xzf {0}/{1} -C {0}".format(
                self._name, archive_name)
        ],
        "{} dependency decompression failed".format(self._target),
    )
    # Clean up the local copy of the archive that is no longer needed.
    RunCommand(
        ["rm", archive_name],
        "{} dependency archive cleanup failed".format(self._target),
    )


def RunTest(target: TestTarget, run_locally: bool = False) -> None:
  linux_out = target.ExecLinux(target_spec.linux_out_dir, run_locally)
  linux_result = test_results.TargetResultFromStdout(linux_out.splitlines(),
                                                     target._name)
  print("Ran Linux")
  fuchsia_out = target.ExecFuchsia(target_spec.fuchsia_out_dir, run_locally)
  fuchsia_result = test_results.TargetResultFromStdout(fuchsia_out.splitlines(),
                                                       target._name)
  print("Ran Fuchsia")
  outname = "{}.{}.json".format(target._name, time.time())
  linux_result.WriteToJson("{}/{}".format(target_spec.raw_linux_dir, outname))
  fuchsia_result.WriteToJson("{}/{}".format(target_spec.raw_fuchsia_dir,
                                            outname))
  print("Wrote result files")


def RunGnForDirectory(dir_name: str, target_os: str) -> None:
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)
  with open("{}/{}".format(dir_name, "args.gn"), "w") as args_file:
    args_file.write("is_debug = false\n")
    args_file.write("dcheck_always_on = false\n")
    args_file.write("is_component_build = false\n")
    args_file.write("use_goma = true\n")
    args_file.write("target_os = \"{}\"\n".format(target_os))

  subprocess.run(["gn", "gen", dir_name]).check_returncode()


def GenerateTestData(runs: int):
  DIR_SOURCE_ROOT = os.path.abspath(
      os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))
  os.chdir(DIR_SOURCE_ROOT)
  os.makedirs(target_spec.results_dir, exist_ok=True)
  os.makedirs(target_spec.raw_linux_dir, exist_ok=True)
  os.makedirs(target_spec.raw_fuchsia_dir, exist_ok=True)
  # Grab parameters from config file.
  linux_dir = target_spec.linux_out_dir
  fuchsia_dir = target_spec.fuchsia_out_dir
  test_input = []  # type: List[TestTarget]
  for target in target_spec.test_targets:
    test_input.append(TestTarget(target))
  print("Test targets collected:\n{}".format("\n".join(
      [test._target for test in test_input])))

  RunGnForDirectory(linux_dir, "linux")
  RunGnForDirectory(fuchsia_dir, "fuchsia")

  # Build test targets in both output directories.
  for directory in [linux_dir, fuchsia_dir]:
    build_command = ["autoninja", "-C", directory] \
                  + [test._target for test in test_input]
    RunCommand(build_command,
               "autoninja failed in directory {}".format(directory))
  print("Builds completed.")

  # Execute the tests, one at a time, per system, and collect their results.
  for i in range(0, runs):
    print("Running Test set {}".format(i))
    for test_target in test_input:
      print("Running Target {}".format(test_target._name))
      RunTest(test_target)
      print("Finished {}".format(test_target._name))

  print("Tests Completed")


def main() -> int:
  GenerateTestData(1)
  return 0


if __name__ == "__main__":
  sys.exit(main())
