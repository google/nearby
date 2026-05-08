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

#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/task_runner.h"
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

TransferManager::TransferManager(
    TaskRunner* absl_nonnull runner, absl::string_view endpoint_id,
    absl::AnyInvocable<void(absl::string_view endpoint_id,
                            std::unique_ptr<Payload> payload)>
        deferred_send_function)
    : runner_(*runner),
      endpoint_id_(endpoint_id),
      deferred_send_function_(std::move(deferred_send_function)) {}

TransferManager::~TransferManager() {
  absl::MutexLock lock(mutex_);
  timeout_timer_.reset();
}

void TransferManager::Send(std::unique_ptr<Payload> payload) {
  absl::MutexLock lock(mutex_);

  if (is_waiting_for_high_quality_medium_) {
    LOG(INFO)
        << "Connection to endpoint " << endpoint_id_
        << " is waiting for a high quality medium, delaying payload transfer.";
    pending_payloads_.push(std::move(payload));
    return;
  }

  deferred_send_function_(endpoint_id_, std::move(payload));
}

void TransferManager::OnMediumQualityChanged(Medium current_medium) {
  absl::MutexLock lock(mutex_);

  if (!is_waiting_for_high_quality_medium_) {
    LOG(WARNING) << "It is not waiting for high quality medium.";
    return;
  }

  if (!IsHighQualityMedium(current_medium)) {
    LOG(WARNING) << "medium switched to low quality Medium: "
                 << static_cast<int>(current_medium);
    return;
  }

  LOG(INFO) << "Connection to endpoint " << endpoint_id_
            << " has changed to a high quality medium: "
            << static_cast<int>(current_medium);
  StopWaitingForHighQualityMedium();
}

bool TransferManager::StartTransfer() {
  absl::MutexLock lock(mutex_);

  if (!is_waiting_for_high_quality_medium_) {
    VLOG(1) << "No need to wait for high quality medium.";
    return false;
  }

  if (timeout_timer_ != nullptr) {
    LOG(WARNING) << "transfer already started.";
    return false;
  }

  timeout_timer_ = std::make_unique<ThreadTimer>(
      runner_, "transfer_manager_timeout_timer", kMediumUpgradeTimeout,
      [this]() {
        absl::MutexLock lock(mutex_);

        LOG(INFO) << "Timed out for endpoint " << endpoint_id_ << " after "
                  << kMediumUpgradeTimeout;
        StopWaitingForHighQualityMedium();
      });

  LOG(INFO) << "Attempting to upgrade the bandwidth for endpoint " +
                   endpoint_id_ + ". Large payloads will be delayed" +
                   " until either bandwidth is upgraded or a timeout of "
            << kMediumUpgradeTimeout << " is reached";
  return true;
}

bool TransferManager::CancelTransfer() {
  absl::MutexLock lock(mutex_);

  if (timeout_timer_ == nullptr) {
    LOG(WARNING) << "No running transfer.";
    return false;
  }

  timeout_timer_.reset();
  LOG(INFO) << __func__ << "Transfer is canceled";
  return true;
}

void TransferManager::StopWaitingForHighQualityMedium() {
  timeout_timer_.reset();
  is_waiting_for_high_quality_medium_ = false;

  LOG(INFO) << "Sending " << pending_payloads_.size()
            << " delayed payloads to endpoint " << endpoint_id_;
  while (!pending_payloads_.empty()) {
    auto payload = std::move(pending_payloads_.front());
    pending_payloads_.pop();
    deferred_send_function_(endpoint_id_, std::move(payload));
  }
}

}  // namespace sharing
}  // namespace nearby
