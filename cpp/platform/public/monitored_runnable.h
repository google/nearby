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

#ifndef PLATFORM_PUBLIC_MONITORED_RUNNABLE_H_
#define PLATFORM_PUBLIC_MONITORED_RUNNABLE_H_

#include <utility>

#include "platform/base/runnable.h"
#include "platform/public/logging.h"
#include "platform/public/pending_job_registry.h"
#include "platform/public/system_clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// A runnable with extra logging
// We log if the task has been waiting long on the executor or if it was running
// for a long time. The latter isn't always an issue - some tasks are expected
// to run for longer periods of time (minutes).
class MonitoredRunnable {
 public:
  explicit MonitoredRunnable(Runnable&& runnable) : runnable_{runnable} {}
  MonitoredRunnable(const std::string& name, Runnable&& runnable)
      : name_{name}, runnable_{runnable} {
    PendingJobRegistry::GetInstance().AddPendingJob(name_, post_time_);
  }

  void operator()() const {
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

 private:
  static constexpr absl::Duration kMinReportedStartDelay = absl::Seconds(5);
  static constexpr absl::Duration kMinReportedTaskDuration = absl::Seconds(10);
  const std::string name_;
  Runnable runnable_;
  absl::Time post_time_ = SystemClock::ElapsedRealtime();
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_MONITORED_RUNNABLE_H_
