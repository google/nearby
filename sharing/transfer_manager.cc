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

#include "sharing/transfer_manager.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/thread_timer.h"

namespace nearby {
namespace sharing {
namespace {

bool IsHighQualityMedium(Medium medium) {
  if (medium == Medium::kWifiLan || medium == Medium::kWifiAware ||
      medium == Medium::kWifiDirect || medium == Medium::kWifiHotspot ||
      medium == Medium::kWebRtc) {
    return true;
  }

  return false;
}

}  // namespace

TransferManager::TransferManager(Context* context,
                                 absl::string_view endpoint_id)
    : context_(context), endpoint_id_(endpoint_id) {}

TransferManager::~TransferManager() {
  absl::MutexLock lock(&mutex_);
  timeout_timer_.reset();
  pending_tasks_.clear();
}

void TransferManager::Send(std::function<void()> task) {
  absl::MutexLock lock(&mutex_);

  if (is_waiting_for_high_quality_medium_) {
    NL_LOG(INFO)
        << "Connection to endpoint " << endpoint_id_
        << " is waiting for a high quality medium, delaying payload transfer.";
    pending_tasks_.push_back(task);
    return;
  }

  task();
}

void TransferManager::OnMediumQualityChanged(Medium current_medium) {
  absl::MutexLock lock(&mutex_);

  if (!is_waiting_for_high_quality_medium_) {
    NL_LOG(WARNING) << "It is not waiting for high quality medium.";
    return;
  }

  if (!IsHighQualityMedium(current_medium)) {
    NL_LOG(WARNING) << "medium switched to low quality Medium: "
                    << static_cast<int>(current_medium);
    return;
  }

  NL_LOG(INFO) << "Connection to endpoint " << endpoint_id_
               << " has changed to a high quality medium: "
               << static_cast<int>(current_medium);
  StopWaitingForHighQualityMedium();
}

bool TransferManager::StartTransfer() {
  absl::MutexLock lock(&mutex_);

  if (!is_waiting_for_high_quality_medium_) {
    NL_LOG(WARNING) << "No need to wait for high quality medium.";
    return false;
  }

  if (timeout_timer_ != nullptr) {
    NL_LOG(WARNING) << "transfer already started.";
    return false;
  }

  timeout_timer_ = std::make_unique<ThreadTimer>(
      *context_->GetTaskRunner(), "transfer_manager_timeout_timer",
      kMediumUpgradeTimeout, [this]() {
        absl::MutexLock lock(&mutex_);

        NL_LOG(INFO) << "Timed out for endpoint " << endpoint_id_ << " after "
                     << kMediumUpgradeTimeout;
        StopWaitingForHighQualityMedium();
      });

  NL_LOG(INFO) << "Attempting to upgrade the bandwidth for endpoint " +
                      endpoint_id_ + ". Large payloads will be delayed" +
                      " until either bandwidth is upgraded or a timeout of "
               << (kMediumUpgradeTimeout / absl::Milliseconds(1))
               << " milliseconds is reached";
  return true;
}

bool TransferManager::CancelTransfer() {
  absl::MutexLock lock(&mutex_);

  if (timeout_timer_ == nullptr) {
    NL_LOG(WARNING) << "No running transfer.";
    return false;
  }

  timeout_timer_.reset();
  NL_LOG(INFO) << __func__ << "Transfer is canceled";
  return true;
}

void TransferManager::StopWaitingForHighQualityMedium() {
  is_waiting_for_high_quality_medium_ = false;

  for (const auto& task : pending_tasks_) {
    NL_LOG(INFO) << "Sending delayed payload to endpoint " << endpoint_id_;
    task();
  }

  pending_tasks_.clear();
  timeout_timer_.reset();
}

}  // namespace sharing
}  // namespace nearby
