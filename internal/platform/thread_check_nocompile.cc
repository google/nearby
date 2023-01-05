// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/time/time.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

// Snippets of invalid code that should trigger an error during thread
// safety analysis at compile time.
// See https://g3doc.corp.google.com/googletest/g3doc/cpp_nc_test.md

namespace nearby {

#ifdef TEST_EXECUTE_MISSING_METHOD_ANNOTATION
struct ThreadCheckTestClass {
  SingleThreadExecutor executor;
  int value ABSL_GUARDED_BY(executor) = 0;

  void incValue() { value++; }
};

void TestExecute_MissingMethodAnnotation() {
  ThreadCheckTestClass test_class;

  test_class.executor.Execute(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
      });
}
#endif  // TEST_EXECUTE_MISSING_METHOD_ANNOTATION

#ifdef TEST_SUBMIT_MISSING_METHOD_ANNOTATION
struct ThreadCheckTestClass {
  SingleThreadExecutor executor;
  int value ABSL_GUARDED_BY(executor) = 0;

  int getValue() { return value; }
};

void TestSubmit_MissingMethodAnnotation() {
  ThreadCheckTestClass test_class;
  Future<int> future;

  test_class.executor.Submit<int>(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        return ExceptionOr<int>{test_class.getValue()};
      },
      &future);
}
#endif  // TEST_SUBMIT_MISSING_METHOD_ANNOTATION

#ifdef TEST_SCHEDULE_MISSING_METHOD_ANNOTATION
struct ThreadCheckTestClass {
  ScheduledExecutor executor;
  int value ABSL_GUARDED_BY(executor) = 0;

  void incValue() { value++; }
};

void TestSchedule_MissingMethodAnnotation() {
  ThreadCheckTestClass test_class;

  test_class.executor.Schedule(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
      },
      absl::ZeroDuration());
}
#endif  // TEST_SCHEDULE_MISSING_METHOD_ANNOTATION

}  // namespace nearby
