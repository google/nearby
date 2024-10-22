// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_TRANSFER_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_TRANSFER_MANAGER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/thread_timer.h"

namespace nearby {
namespace sharing {

// TransferManager is used to delay the payload transfer until the medium
// quality is in high quality. If the quality doesn't change in a duration, it
// will give up to wait for the medium change.
class TransferManager {
 public:
  // Used to wait for the medium upgrade.
  static constexpr absl::Duration kMediumUpgradeTimeout = absl::Seconds(10);

  TransferManager(Context* context, absl::string_view endpoint_id);

  ~TransferManager();

  void Send(std::function<void()> task) ABSL_LOCKS_EXCLUDED(mutex_);
  void OnMediumQualityChanged(Medium current_medium)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StartTransfer() ABSL_LOCKS_EXCLUDED(mutex_);
  bool CancelTransfer() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void StopWaitingForHighQualityMedium() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Context* context_;
  std::string endpoint_id_;
  absl::Mutex mutex_;
  bool is_waiting_for_high_quality_medium_ ABSL_GUARDED_BY(mutex_) = true;
  std::vector<std::function<void()>> pending_tasks_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<ThreadTimer> timeout_timer_ ABSL_GUARDED_BY(mutex_) = nullptr;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_TRANSFER_MANAGER_H_
