# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from telemetry import decorators
from telemetry.testing import options_for_unittests


class ScriptsSmokeTest(unittest.TestCase):

  perf_dir = os.path.dirname(__file__)

  def RunPerfScript(self, command):
    main_command = [sys.executable]
    args = main_command + command.split(' ')
    proc = subprocess.Popen(args, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, cwd=self.perf_dir)
    stdout = proc.communicate()[0]
    return_code = proc.returncode
    return return_code, stdout

  def testRunBenchmarkHelp(self):
    return_code, stdout = self.RunPerfScript('run_benchmark help')
    self.assertEquals(return_code, 0, stdout)
    self.assertIn('Available commands are', stdout)

  def testRunBenchmarkRunListsOutBenchmarks(self):
    return_code, stdout = self.RunPerfScript('run_benchmark run')
    self.assertIn('Pass --browser to list benchmarks', stdout)
    self.assertNotEquals(return_code, 0)

  def testRunBenchmarkRunNonExistingBenchmark(self):
    return_code, stdout = self.RunPerfScript('run_benchmark foo')
    self.assertIn('No benchmark named "foo"', stdout)
    self.assertNotEquals(return_code, 0)

  def testRunRecordWprHelp(self):
    return_code, stdout = self.RunPerfScript('record_wpr')
    self.assertEquals(return_code, 0, stdout)
    self.assertIn('optional arguments:', stdout)

  @decorators.Disabled('chromeos')  # crbug.com/814068
  def testRunRecordWprList(self):
    return_code, stdout = self.RunPerfScript('record_wpr --list-benchmarks')
    # TODO(nednguyen): Remove this once we figure out why importing
    # small_profile_extender fails on Android dbg.
    # crbug.com/561668
    if 'ImportError: cannot import name small_profile_extender' in stdout:
      self.skipTest('small_profile_extender is missing')
    self.assertEquals(return_code, 0, stdout)
    self.assertIn('kraken', stdout)

  @decorators.Disabled('chromeos')  # crbug.com/754913
  def testRunTelemetryBenchmarkAsGoogletest(self):
    options = options_for_unittests.GetCopy()
    browser_type = options.browser_type
    tempdir = tempfile.mkdtemp()
    benchmark = 'dummy_benchmark.stable_benchmark_1'
    return_code, stdout = self.RunPerfScript(
        '../../testing/scripts/run_performance_tests.py '
        '../../tools/perf/run_benchmark '
        '--benchmarks=dummy_benchmark.stable_benchmark_1 '
        '--browser=%s '
        '--isolated-script-test-repeat=2 '
        '--isolated-script-test-also-run-disabled-tests '
        '--isolated-script-test-output=%s' % (
            browser_type,
            os.path.join(tempdir, 'output.json')
        ))
    self.assertEquals(return_code, 0, stdout)
    try:
      # By design, run_performance_tests.py does not output test results
      # to the location passed in by --isolated-script-test-output. Instead
      # it uses that directory of that file and puts stuff in its own
      # subdirectories for the purposes of merging later.
      with open(os.path.join(tempdir, benchmark, 'test_results.json')) as f:
        test_results = json.load(f)
        self.assertIsNotNone(
            test_results, 'json_test_results should be populated: ' + stdout)
        test_repeats = test_results['num_failures_by_type']['PASS']
        self.assertEqual(
            test_repeats, 2, '--isolated-script-test-repeat=2 should work.')
    except IOError as e:
      self.fail('json_test_results should be populated: ' + stdout + str(e))
    finally:
      shutil.rmtree(tempdir)

