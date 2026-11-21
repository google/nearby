// Copyright 2026 Google LLC
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

#include "internal/platform/monitored_runnable.h"

#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "internal/platform/runnable.h"

#if !defined(NEARBY_CHROMIUM)
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "absl/base/log_severity.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/pending_job_registry.h"
#endif  // !defined(NEARBY_CHROMIUM)

namespace nearby {
namespace {

static_assert(!std::is_copy_constructible_v<MonitoredRunnable>);
static_assert(!std::is_copy_assignable_v<MonitoredRunnable>);
static_assert(!std::is_move_constructible_v<MonitoredRunnable>);
static_assert(!std::is_move_assignable_v<MonitoredRunnable>);

TEST(MonitoredRunnableTest, UnnamedRunnableExecutesTask) {
  bool executed = false;
  Runnable runnable =
      MakeMonitoredRunnable([&executed]() { executed = true; });

  runnable();

  EXPECT_TRUE(executed);
}

TEST(MonitoredRunnableTest, EmptyRunnableReturnsNull) {
  EXPECT_FALSE(MakeMonitoredRunnable(nullptr));
  EXPECT_FALSE(MakeMonitoredRunnable("empty_job", nullptr));
}

#if !defined(NEARBY_CHROMIUM)
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;

// Returns true if `PendingJobRegistry::ListAllJobs()` logs a waiting entry for
// `job_name`.
bool IsJobPendingInRegistry(absl::string_view job_name) {
  bool is_pending = false;
  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _,
                       HasSubstr(absl::StrCat("Task \"", job_name, "."))))
      .Times(AnyNumber())
      .WillRepeatedly([&is_pending] { is_pending = true; });
  log.StartCapturingLogs();
  PendingJobRegistry::GetInstance().ListAllJobs();
  return is_pending;
}

TEST(MonitoredRunnableTest, DestructorRemovesUnrunJobFromRegistry) {
  constexpr absl::string_view kJobName = "monitored_runnable_unrun_job";
  bool executed = false;
  {
    Runnable runnable =
        MakeMonitoredRunnable(kJobName, [&executed]() { executed = true; });
    EXPECT_TRUE(IsJobPendingInRegistry(kJobName));
  }

  EXPECT_FALSE(executed);
  EXPECT_FALSE(IsJobPendingInRegistry(kJobName));
}

TEST(MonitoredRunnableTest, MovingWrappedRunnablePreservesPendingJobOwnership) {
  constexpr absl::string_view kTargetJobName =
      "monitored_runnable_move_target_job";
  constexpr absl::string_view kSourceJobName =
      "monitored_runnable_move_source_job";
  bool target_executed = false;
  bool source_executed = false;
  {
    Runnable target = MakeMonitoredRunnable(
        kTargetJobName, [&target_executed]() { target_executed = true; });
    {
      Runnable source = MakeMonitoredRunnable(
          kSourceJobName, [&source_executed]() { source_executed = true; });
      EXPECT_TRUE(IsJobPendingInRegistry(kTargetJobName));
      EXPECT_TRUE(IsJobPendingInRegistry(kSourceJobName));
      target = std::move(source);
    }
    EXPECT_FALSE(IsJobPendingInRegistry(kTargetJobName));
    EXPECT_TRUE(IsJobPendingInRegistry(kSourceJobName));

    target();

    EXPECT_TRUE(source_executed);
    EXPECT_FALSE(target_executed);
    EXPECT_FALSE(IsJobPendingInRegistry(kSourceJobName));
  }

  EXPECT_FALSE(IsJobPendingInRegistry(kTargetJobName));
  EXPECT_FALSE(IsJobPendingInRegistry(kSourceJobName));
}

TEST(MonitoredRunnableTest, ExecutionRunsTaskAndRemovesPendingJobFromRegistry) {
  constexpr absl::string_view kJobName = "monitored_runnable_executed_job";
  int execution_count = 0;
  {
    Runnable runnable = MakeMonitoredRunnable(
        kJobName, [&execution_count]() { ++execution_count; });
    EXPECT_TRUE(IsJobPendingInRegistry(kJobName));

    runnable();

    EXPECT_EQ(execution_count, 1);
    EXPECT_FALSE(IsJobPendingInRegistry(kJobName));
  }

  EXPECT_EQ(execution_count, 1);
  EXPECT_FALSE(IsJobPendingInRegistry(kJobName));
}
#endif  // !defined(NEARBY_CHROMIUM)

}  // namespace
}  // namespace nearby
