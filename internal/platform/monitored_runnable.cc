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

#include "internal/platform/monitored_runnable.h"

#include <utility>

#include "internal/platform/logging.h"
#include "internal/platform/pending_job_registry.h"

namespace nearby {

namespace {
absl::Duration kMinReportedStartDelay = absl::Seconds(5);
absl::Duration kMinReportedTaskDuration = absl::Seconds(10);
}  // namespace

MonitoredRunnable::MonitoredRunnable(Runnable&& runnable)
    : runnable_{std::move(runnable)} {}

MonitoredRunnable::MonitoredRunnable(const std::string& name,
                                     Runnable&& runnable)
    : name_{name}, runnable_{std::move(runnable)} {
  PendingJobRegistry::GetInstance().AddPendingJob(name_, post_time_);
}

void MonitoredRunnable::operator()() {
  auto start_time = SystemClock::ElapsedRealtime();
  auto start_delay = start_time - post_time_;
  if (start_delay >= kMinReportedStartDelay) {
    NEARBY_LOGS(INFO) << "Task: \"" << name_ << "\" started after "
                      << absl::ToInt64Seconds(start_delay) << " seconds";
  }
  PendingJobRegistry::GetInstance().RemovePendingJob(name_, post_time_);
  PendingJobRegistry::GetInstance().AddRunningJob(name_, post_time_);
  runnable_();
  auto task_duration = SystemClock::ElapsedRealtime() - start_time;
  if (task_duration >= kMinReportedTaskDuration) {
    NEARBY_LOGS(INFO) << "Task: \"" << name_ << "\" finished after "
                      << absl::ToInt64Seconds(task_duration) << " seconds";
  }
  PendingJobRegistry::GetInstance().RemoveRunningJob(name_, post_time_);
  PendingJobRegistry::GetInstance().ListJobs();
}

}  // namespace nearby
