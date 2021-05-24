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

#include "platform/public/monitored_runnable.h"

#include "platform/public/logging.h"
#include "platform/public/pending_job_registry.h"

namespace location {
namespace nearby {

namespace {
absl::Duration kMinReportedStartDelay = absl::Seconds(5);
absl::Duration kMinReportedTaskDuration = absl::Seconds(10);
}  // namespace

MonitoredRunnable::MonitoredRunnable(Runnable&& runnable)
    : runnable_{runnable} {}

MonitoredRunnable::MonitoredRunnable(const std::string& name,
                                     Runnable&& runnable)
    : name_{name}, runnable_{runnable} {
  PendingJobRegistry::GetInstance().AddPendingJob(name_, post_time_);
}

MonitoredRunnable::~MonitoredRunnable() = default;

void MonitoredRunnable::operator()() const {
  auto start_time = SystemClock::ElapsedRealtime();
  auto start_delay = start_time - post_time_;
  if (start_delay >= kMinReportedStartDelay) {
    NEARBY_LOG(INFO, "Task \"%s\" started after %" PRIu64 " seconds.",
               name_.c_str(), absl::ToInt64Seconds(start_delay));
  }
  PendingJobRegistry::GetInstance().RemovePendingJob(name_, post_time_);
  PendingJobRegistry::GetInstance().AddRunningJob(name_, post_time_);
  runnable_();
  auto task_duration = SystemClock::ElapsedRealtime() - start_time;
  if (task_duration >= kMinReportedTaskDuration) {
    NEARBY_LOG(INFO, "Task \"%s\" finished after %" PRIu64 " seconds.",
               name_.c_str(), absl::ToInt64Seconds(task_duration));
  }
  PendingJobRegistry::GetInstance().RemoveRunningJob(name_, post_time_);
  PendingJobRegistry::GetInstance().ListJobs();
}

}  // namespace nearby
}  // namespace location
