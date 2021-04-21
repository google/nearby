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

#ifndef PLATFORM_PUBLIC_PENDING_JOB_REGISTRY_H_
#define PLATFORM_PUBLIC_PENDING_JOB_REGISTRY_H_

#include "platform/public/logging.h"
#include "platform/public/mutex.h"
#include "platform/public/mutex_lock.h"
#include "platform/public/system_clock.h"
#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// A global registry of running tasks. The goal is to help us monitor
// tasks that are either waiting too long for their turn or they never finish
class PendingJobRegistry {
 public:
  static PendingJobRegistry& GetInstance() {
    static PendingJobRegistry* instance = new PendingJobRegistry();
    return *instance;
  }

  void AddPendingJob(const std::string& name, absl::Time post_time) {
    MutexLock lock(&mutex_);
    pending_jobs_.emplace(CreateKey(name, post_time), post_time);
  }

  void RemovePendingJob(const std::string& name, absl::Time post_time) {
    MutexLock lock(&mutex_);
    pending_jobs_.erase(CreateKey(name, post_time));
  }

  void AddRunningJob(const std::string& name, absl::Time post_time) {
    MutexLock lock(&mutex_);
    running_jobs_.emplace(CreateKey(name, post_time),
                          SystemClock::ElapsedRealtime());
  }

  void RemoveRunningJob(const std::string& name, absl::Time post_time) {
    MutexLock lock(&mutex_);
    running_jobs_.erase(CreateKey(name, post_time));
  }

  void ListJobs() {
    auto current_time = SystemClock::ElapsedRealtime();
    if (current_time - list_jobs_time_ < kMinReportInterval) return;
    MutexLock lock(&mutex_);
    for (auto& job : pending_jobs_) {
      auto age = current_time - job.second;
      if (age >= kReportPendingJobsOlderThan) {
        NEARBY_LOGS(INFO) << "Task \"" << job.first << "\" is waiting for "
                          << absl::ToInt64Seconds(age) << " s";
      }
    }
    for (auto& job : running_jobs_) {
      auto age = current_time - job.second;
      if (age >= kReportRunningJobsOlderThan) {
        NEARBY_LOGS(INFO) << "Task \"" << job.first << "\" is running for "
                          << absl::ToInt64Seconds(age) << " s";
      }
    }
    list_jobs_time_ = current_time;
  }

 private:
  PendingJobRegistry() = default;
  static constexpr absl::Duration kMinReportInterval = absl::Seconds(60);
  static constexpr absl::Duration kReportPendingJobsOlderThan =
      absl::Seconds(40);
  static constexpr absl::Duration kReportRunningJobsOlderThan =
      absl::Seconds(60);

  std::string CreateKey(const std::string& name, absl::Time post_time) {
    return name + "." + std::to_string(absl::ToUnixNanos(post_time));
  }

  Mutex mutex_;
  absl::flat_hash_map<const std::string, absl::Time> pending_jobs_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<const std::string, absl::Time> running_jobs_
      ABSL_GUARDED_BY(mutex_);
  absl::Time list_jobs_time_ = absl::UnixEpoch();
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_PENDING_JOB_REGISTRY_H_
