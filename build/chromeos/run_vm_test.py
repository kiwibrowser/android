#!/usr/bin/env vpython
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import json
import logging
import os
import re
import signal
import stat
import sys

import psutil

CHROMIUM_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

# Use the android test-runner's gtest results support library for generating
# output json ourselves.
sys.path.insert(0, os.path.join(CHROMIUM_SRC_PATH, 'build', 'android'))
from pylib.base import base_test_result
from pylib.results import json_results

# Use luci-py's subprocess42.py
sys.path.insert(
    0, os.path.join(CHROMIUM_SRC_PATH, 'tools', 'swarming_client', 'utils'))
import subprocess42

CHROMITE_PATH = os.path.abspath(os.path.join(
    CHROMIUM_SRC_PATH, 'third_party', 'chromite'))
CROS_RUN_VM_TEST_PATH = os.path.abspath(os.path.join(
    CHROMITE_PATH, 'bin', 'cros_run_vm_test'))


_FILE_BLACKLIST = [
  re.compile(r'.*build/chromeos.*'),
  re.compile(r'.*build/cros_cache.*'),
  re.compile(r'.*third_party/chromite.*'),
]


def read_runtime_files(runtime_deps_path, outdir):
  if not runtime_deps_path:
    return []

  abs_runtime_deps_path = os.path.abspath(
      os.path.join(outdir, runtime_deps_path))
  with open(abs_runtime_deps_path) as runtime_deps_file:
    files = [l.strip() for l in runtime_deps_file if l]
  rel_file_paths = []
  for f in files:
    rel_file_path = os.path.relpath(
        os.path.abspath(os.path.join(outdir, f)),
        os.getcwd())
    if not any(regex.match(rel_file_path) for regex in _FILE_BLACKLIST):
      rel_file_paths.append(rel_file_path)

  return rel_file_paths


def host_cmd(args, unknown_args):
  if not args.cmd:
    logging.error('Must specify command to run on the host.')
    return 1
  elif unknown_args:
    logging.error(
        'Args "%s" unsupported. Is your host command correctly formatted?',
        ' '.join(unknown_args))
    return 1

  cros_run_vm_test_cmd = [
      CROS_RUN_VM_TEST_PATH,
      '--start',
      '--board', args.board,
      '--cache-dir', args.cros_cache,
  ]
  if args.verbose:
    cros_run_vm_test_cmd.append('--debug')

  cros_run_vm_test_cmd += [
      '--host-cmd',
      '--',
  ] + args.cmd

  logging.info('Running the following command:')
  logging.info(' '.join(cros_run_vm_test_cmd))

  return subprocess42.call(
      cros_run_vm_test_cmd, stdout=sys.stdout, stderr=sys.stderr)


def vm_test(args, unknown_args):
  is_sanity_test = args.test_exe == 'cros_vm_sanity_test'

  # To keep things easy for us, ensure both types of output locations are
  # the same.
  if args.test_launcher_summary_output and args.vm_logs_dir:
    json_output_dir = os.path.dirname(args.test_launcher_summary_output) or '.'
    if os.path.abspath(json_output_dir) != os.path.abspath(args.vm_logs_dir):
      logging.error(
          '--test-launcher-summary-output and --vm-logs-dir must point to '
          'the same directory.')
      return 1

  cros_run_vm_test_cmd = [
      CROS_RUN_VM_TEST_PATH,
      '--start',
      '--board', args.board,
      '--cache-dir', args.cros_cache,
  ]

  # cros_run_vm_test has trouble with relative paths that go up directories, so
  # cd to src/, which should be the root of all data deps.
  os.chdir(CHROMIUM_SRC_PATH)

  runtime_files = read_runtime_files(
      args.runtime_deps_path, args.path_to_outdir)
  if args.vpython_dir:
    # --vpython-dir is relative to the out dir, but --files expects paths
    # relative to src dir, so fix the path up a bit.
    runtime_files.append(
        os.path.relpath(
            os.path.abspath(os.path.join(args.path_to_outdir,
                                         args.vpython_dir)),
            CHROMIUM_SRC_PATH))
    runtime_files.append('.vpython')

  # If we're pushing files, we need to set the cwd.
  if runtime_files:
      cros_run_vm_test_cmd.extend(
          ['--cwd', os.path.relpath(args.path_to_outdir, CHROMIUM_SRC_PATH)])
  for f in runtime_files:
    cros_run_vm_test_cmd.extend(['--files', f])

  if args.vm_logs_dir:
    cros_run_vm_test_cmd += [
        '--results-src', '/var/log/',
        '--results-dest-dir', args.vm_logs_dir,
    ]

  if args.test_launcher_summary_output and not is_sanity_test:
    result_dir, result_file = os.path.split(args.test_launcher_summary_output)
    # If args.test_launcher_summary_output is a file in cwd, result_dir will be
    # an empty string, so replace it with '.' when this is the case so
    # cros_run_vm_test can correctly handle it.
    if not result_dir:
      result_dir = '.'
    vm_result_file = '/tmp/%s' % result_file
    cros_run_vm_test_cmd += [
      '--results-src', vm_result_file,
      '--results-dest-dir', result_dir,
    ]

  if is_sanity_test:
    # run_cros_vm_test's default behavior when no cmd is specified is the sanity
    # test that's baked into the VM image. This test smoke-checks the system
    # browser, so deploy our locally-built chrome to the VM before testing.
    cros_run_vm_test_cmd += [
        '--deploy',
        '--build-dir', os.path.relpath(args.path_to_outdir, CHROMIUM_SRC_PATH),
    ]
  else:
    pre_test_cmds = [
        # /home is mounted with "noexec" in the VM, but some of our tools
        # and tests use the home dir as a workspace (eg: vpython downloads
        # python binaries to ~/.vpython-root). /tmp doesn't have this
        # restriction, so change the location of the home dir for the
        # duration of the test.
        'export HOME=/tmp', '\;',
    ]
    if args.vpython_dir:
      vpython_spec_path = os.path.relpath(
          os.path.join(CHROMIUM_SRC_PATH, '.vpython'),
          args.path_to_outdir)
      pre_test_cmds += [
          # Backslash is needed to prevent $PATH from getting prematurely
          # executed on the host.
          'export PATH=\$PATH:\$PWD/%s' % args.vpython_dir, '\;',
          # Initialize the vpython cache. This can take 10-20s, and some tests
          # can't afford to wait that long on the first invocation.
          'vpython', '-vpython-spec', vpython_spec_path, '-vpython-tool',
          'install', '\;',
      ]
    cros_run_vm_test_cmd += [
        # Some tests fail as root, so run as the less privileged user 'chronos'.
        '--as-chronos',
        '--cmd',
        '--',
        # Wrap the cmd to run in the VM around quotes (") so that the
        # interpreter on the host doesn't stop at any ";" or "&&" tokens in the
        # cmd.
        '"',
    ] + pre_test_cmds + [
        './' + args.test_exe,
        '--test-launcher-shard-index=%d' % args.test_launcher_shard_index,
        '--test-launcher-total-shards=%d' % args.test_launcher_total_shards,
    ] + unknown_args + [
        '"',
    ]

  if args.test_launcher_summary_output and not is_sanity_test:
    cros_run_vm_test_cmd += [
      '--test-launcher-summary-output=%s' % vm_result_file,
    ]

  logging.info('Running the following command:')
  logging.info(' '.join(cros_run_vm_test_cmd))

  # deploy_chrome needs a set of GN args used to build chrome to determine if
  # certain libraries need to be pushed to the VM. It looks for the args via an
  # env var. To trigger the default deploying behavior, give it a dummy set of
  # args.
  # TODO(crbug.com/823996): Make the GN-dependent deps controllable via cmd-line
  # args.
  env_copy = os.environ.copy()
  if not env_copy.get('GN_ARGS'):
    env_copy['GN_ARGS'] = 'is_chromeos = true'
  env_copy['PATH'] = env_copy['PATH'] + ':' + os.path.join(CHROMITE_PATH, 'bin')

  # Traps SIGTERM and kills all child processes of cros_run_vm_test when it's
  # caught. This will allow us to capture logs from the VM if a test hangs
  # and gets timeout-killed by swarming. See also:
  # https://chromium.googlesource.com/infra/luci/luci-py/+/master/appengine/swarming/doc/Bot.md#graceful-termination_aka-the-sigterm-and-sigkill-dance
  test_proc = None
  def _kill_child_procs(trapped_signal, _):
    logging.warning(
        'Received signal %d. Killing child processes of test.', trapped_signal)
    if not test_proc or not test_proc.pid:
      # This shouldn't happen?
      logging.error('Test process not running.')
      return
    for child in psutil.Process(test_proc.pid).children():
      logging.warning('Killing process %s', child)
      child.kill()

  # Standard GTests should handle retries and timeouts themselves.
  retries, timeout = 0, None
  if is_sanity_test:
    # 5 min should be enough time for the sanity test to pass.
    retries, timeout = 2, 300
  signal.signal(signal.SIGTERM, _kill_child_procs)

  for i in xrange(retries+1):
    logging.info('########################################')
    logging.info('Test attempt #%d', i)
    logging.info('########################################')
    test_proc = subprocess42.Popen(
        cros_run_vm_test_cmd, stdout=sys.stdout, stderr=sys.stderr,
        env=env_copy)
    try:
      test_proc.wait(timeout=timeout)
    except subprocess42.TimeoutExpired:
      logging.error('Test timed out. Sending SIGTERM.')
      # SIGTERM the proc and wait 10s for it to close.
      test_proc.terminate()
      try:
        test_proc.wait(timeout=10)
      except subprocess42.TimeoutExpired:
        # If it hasn't closed in 10s, SIGKILL it.
        logging.error('Test did not exit in time. Sending SIGKILL.')
        test_proc.kill()
        test_proc.wait()
    logging.info('Test exitted with %d.', test_proc.returncode)
    if test_proc.returncode == 0:
      break

  rc = test_proc.returncode

  # Create a simple json results file for the sanity test if needed. The results
  # will contain only one test ('cros_vm_sanity_test'), and will either be a
  # PASS or FAIL depending on the return code of cros_run_vm_test above.
  if args.test_launcher_summary_output and is_sanity_test:
    result = (base_test_result.ResultType.FAIL if rc else
                  base_test_result.ResultType.PASS)
    sanity_test_result = base_test_result.BaseTestResult(
        'cros_vm_sanity_test', result)
    run_results = base_test_result.TestRunResults()
    run_results.AddResult(sanity_test_result)
    with open(args.test_launcher_summary_output, 'w') as f:
      json.dump(json_results.GenerateResultsDict([run_results]), f)

  return rc


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose', '-v', action='store_true')
  # Required args.
  parser.add_argument(
      '--board', type=str, required=True, help='Type of CrOS device.')
  subparsers = parser.add_subparsers(dest='test_type')
  # Host-side test args.
  host_cmd_parser = subparsers.add_parser(
      'host-cmd',
      help='Runs a host-side test. Pass the host-side command to run after '
           '"--". Hostname and port for the VM will be 127.0.0.1:9222.')
  host_cmd_parser.set_defaults(func=host_cmd)
  host_cmd_parser.add_argument(
      '--cros-cache', type=str, required=True, help='Path to cros cache.')
  host_cmd_parser.add_argument('cmd', nargs=argparse.REMAINDER)
  # VM-side test args.
  vm_test_parser = subparsers.add_parser(
      'vm-test',
      help='Runs a vm-side gtest.')
  vm_test_parser.set_defaults(func=vm_test)
  vm_test_parser.add_argument(
      '--cros-cache', type=str, required=True, help='Path to cros cache.')
  vm_test_parser.add_argument(
      '--test-exe', type=str, required=True,
      help='Path to test executable to run inside VM. If the value is '
           '"cros_vm_sanity_test", the sanity test that ships with the VM '
           'image runs instead. This test smokes-check the system browser '
           '(eg: loads a simple webpage, executes some javascript), so a '
           'fully-built Chrome binary that can get deployed to the VM is '
           'expected to available in the out-dir.')

  # GTest args. Some are passed down to the test binary in the VM. Others are
  # parsed here since they might need tweaking or special handling.
  vm_test_parser.add_argument(
      '--test-launcher-summary-output', type=str,
      help='When set, will pass the same option down to the test and retrieve '
           'its result file at the specified location.')
  # Shard args are parsed here since we might also specify them via env vars.
  vm_test_parser.add_argument(
      '--test-launcher-shard-index',
      type=int, default=os.environ.get('GTEST_SHARD_INDEX', 0),
      help='Index of the external shard to run.')
  vm_test_parser.add_argument(
      '--test-launcher-total-shards',
      type=int, default=os.environ.get('GTEST_TOTAL_SHARDS', 1),
      help='Total number of external shards.')

  # Misc args.
  vm_test_parser.add_argument(
      '--path-to-outdir', type=str, required=True,
      help='Path to output directory, all of whose contents will be deployed '
           'to the device.')
  vm_test_parser.add_argument(
      '--runtime-deps-path', type=str,
      help='Runtime data dependency file from GN.')
  vm_test_parser.add_argument(
      '--vpython-dir', type=str,
      help='Location on host of a directory containing a vpython binary to '
           'deploy to the VM before the test starts. The location of this dir '
           'will be added onto PATH in the VM. WARNING: The arch of the VM '
           'might not match the arch of the host, so avoid using "${platform}" '
           'when downloading vpython via CIPD.')
  vm_test_parser.add_argument(
      '--vm-logs-dir', type=str,
      help='Will copy everything under /var/log/ from the VM after the test '
           'into the specified dir.')
  args, unknown_args = parser.parse_known_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARN)

  if not os.path.exists('/dev/kvm'):
    logging.error('/dev/kvm is missing. Is KVM installed on this machine?')
    return 1
  elif not os.access('/dev/kvm', os.W_OK):
    logging.error(
        '/dev/kvm is not writable as current user. Perhaps you should be root?')
    return 1

  args.cros_cache = os.path.abspath(args.cros_cache)
  return args.func(args, unknown_args)


if __name__ == '__main__':
  sys.exit(main())
