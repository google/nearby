// Copyright 2022-2023 Google LLC
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

#include "sharing/nearby_connection_impl.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "internal/platform/device_info.h"
#include "sharing/internal/public/logging.h"

namespace nearby::sharing {

NearbyConnectionImpl::NearbyConnectionImpl(nearby::DeviceInfo& device_info)
    : device_info_(device_info) {
  if (!device_info_.PreventSleep()) {
    LOG(WARNING) << __func__ << ":Failed to prevent device sleep.";
  }
}

NearbyConnectionImpl::~NearbyConnectionImpl() {
  std::function<void()> disconnect_listener;
  std::function<void(std::optional<std::vector<uint8_t>> bytes)> read_callback;
  {
    absl::MutexLock lock(mutex_);
    if (!device_info_.AllowSleep()) {
      LOG(ERROR) << __func__ << ":Failed to allow device sleep.";
    }
    disconnect_listener = std::move(disconnect_listener_);
    read_callback = std::move(read_callback_);
  }
  if (disconnect_listener) {
    disconnect_listener();
  }

  if (read_callback) {
    read_callback(std::nullopt);
  }
}

void NearbyConnectionImpl::Read(
    std::function<void(std::optional<std::vector<uint8_t>> bytes)> callback) {
  std::vector<uint8_t> bytes;
  {
    absl::MutexLock lock(mutex_);
    if (reads_.empty()) {
      read_callback_ = std::move(callback);
      return;
    }

    bytes = std::move(reads_.front());
    reads_.pop();
  }
  std::move(callback)(std::move(bytes));
}

void NearbyConnectionImpl::SetDisconnectionListener(
    std::function<void()> listener) {
  absl::MutexLock lock(mutex_);
  disconnect_listener_ = std::move(listener);
}

void NearbyConnectionImpl::WriteMessage(std::vector<uint8_t> bytes) {
  std::function<void(std::optional<std::vector<uint8_t>> bytes)> read_callback;
  {
    absl::MutexLock lock(mutex_);
    if (!read_callback_) {
      reads_.push(std::move(bytes));
      return;
    }
    read_callback = std::move(read_callback_);
    read_callback_ = nullptr;
  }
  if (read_callback) {
    read_callback(std::move(bytes));
  }
}

}  // namespace nearby::sharing
