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

#include "platform/public/pending_job_registry.h"

#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"
#include "platform/public/system_clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

namespace {
absl::Duration kMinReportInterval = absl::Seconds(60);
absl::Duration kReportPendingJobsOlderThan = absl::Seconds(40);
absl::Duration kReportRunningJobsOlderThan = absl::Seconds(60);
}  // namespace

PendingJobRegistry& PendingJobRegistry::GetInstance() {
  static PendingJobRegistry* instance = new PendingJobRegistry();
  return *instance;
}

PendingJobRegistry::PendingJobRegistry() = default;

PendingJobRegistry::~PendingJobRegistry() = default;

void PendingJobRegistry::AddPendingJob(const std::string& name,
                                       absl::Time post_time) {
  MutexLock lock(&mutex_);
  pending_jobs_.emplace(CreateKey(name, post_time), post_time);
}

void PendingJobRegistry::RemovePendingJob(const std::string& name,
                                          absl::Time post_time) {
  MutexLock lock(&mutex_);
  pending_jobs_.erase(CreateKey(name, post_time));
}

void PendingJobRegistry::AddRunningJob(const std::string& name,
                                       absl::Time post_time) {
  MutexLock lock(&mutex_);
  running_jobs_.emplace(CreateKey(name, post_time),
                        SystemClock::ElapsedRealtime());
}

void PendingJobRegistry::RemoveRunningJob(const std::string& name,
                                          absl::Time post_time) {
  MutexLock lock(&mutex_);
  running_jobs_.erase(CreateKey(name, post_time));
}

void PendingJobRegistry::ListJobs() {
  auto current_time = SystemClock::ElapsedRealtime();
  MutexLock lock(&mutex_);
  if (current_time - list_jobs_time_ < kMinReportInterval)
    return;
  for (auto& job : pending_jobs_) {
    auto age = current_time - job.second;
    if (age >= kReportPendingJobsOlderThan) {
      NEARBY_LOG(INFO, "Task \"%s\" is waiting for %" PRIu64 " s",
                 job.first.c_str(), absl::ToInt64Seconds(age));
    }
  }
  for (auto& job : running_jobs_) {
    auto age = current_time - job.second;
    if (age >= kReportRunningJobsOlderThan) {
      NEARBY_LOG(INFO, "Task \"%s\" is running for %" PRIu64 " s",
                 job.first.c_str(), absl::ToInt64Seconds(age));
    }
  }
  list_jobs_time_ = current_time;
}

std::string PendingJobRegistry::CreateKey(const std::string& name,
                                          absl::Time post_time) {
  return name + "." + std::to_string(absl::ToUnixNanos(post_time));
}

}  // namespace nearby
}  // namespace location
