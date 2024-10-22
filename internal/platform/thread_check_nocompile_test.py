# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Negative tests for thread safety analysis in executors implementation."""

from google3.testing.pybase import fake_target_util
from google3.testing.pybase import googletest


class ThreadCheckNocompileTest(googletest.TestCase):
  """Negative tests for thread safety analysis in executors implementation."""

  def testCompilerErrors(self):
    """Runs a list of tests to verify that erroneous code leads to expected compiler messages."""
    test_specs = [
        ('EXECUTE_MISSING_METHOD_ANNOTATION', [r'-Wthread-safety-analysis']),
        ('SUBMIT_MISSING_METHOD_ANNOTATION', [r'-Wthread-safety-analysis']),
        ('SCHEDULE_MISSING_METHOD_ANNOTATION', [r'-Wthread-safety-analysis']),
        # Tests that compiling a valid C++ succeeds.
        ('SANITY', None)  # None means that the compilation should succeed.
    ]
    fake_target_util.AssertCcCompilerErrors(
        self,
        'internal/platform/thread_check_nocompile',
        'thread_check_nocompile.o', test_specs)


if __name__ == '__main__':
  googletest.main()
