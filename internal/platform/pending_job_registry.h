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

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/mutex.h"

namespace nearby {

// A global registry of running tasks. The goal is to help us monitor
// tasks that are either waiting too long for their turn or they never finish
class PendingJobRegistry {
 public:
  static PendingJobRegistry& GetInstance();

  ~PendingJobRegistry();

  void AddPendingJob(const std::string& name, absl::Time post_time);
  void RemovePendingJob(const std::string& name, absl::Time post_time);
  void AddRunningJob(const std::string& name, absl::Time post_time);
  void RemoveRunningJob(const std::string& name, absl::Time post_time);
  void ListJobs();
  void ListAllJobs();

 private:
  PendingJobRegistry();

  std::string CreateKey(const std::string& name, absl::Time post_time);

  Mutex mutex_;
  absl::flat_hash_map<std::string, absl::Time> pending_jobs_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, absl::Time> running_jobs_
      ABSL_GUARDED_BY(mutex_);
  absl::Time list_jobs_time_ ABSL_GUARDED_BY(mutex_) = absl::UnixEpoch();
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_PENDING_JOB_REGISTRY_H_
