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
