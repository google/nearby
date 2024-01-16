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
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/device_info.h"
#include "internal/platform/mutex_lock.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

NearbyConnectionImpl::NearbyConnectionImpl(
    nearby::DeviceInfo& device_info,
    NearbyConnectionsManager* nearby_connections_manager,
    absl::string_view endpoint_id)
    : device_info_(device_info),
      nearby_connections_manager_(nearby_connections_manager),
      endpoint_id_(endpoint_id) {
  if (!device_info_.PreventSleep()) {
    NL_LOG(WARNING) << __func__ << ":Failed to prevent device sleep.";
  }
}

NearbyConnectionImpl::~NearbyConnectionImpl() {
  MutexLock lock(&mutex_);
  if (!device_info_.AllowSleep()) {
    NL_LOG(ERROR) << __func__ << ":Failed to allow device sleep.";
  }

  if (disconnect_listener_) {
    disconnect_listener_();
  }

  if (read_callback_) {
    read_callback_(std::nullopt);
  }
}

void NearbyConnectionImpl::Read(ReadCallback callback) {
  MutexLock lock(&mutex_);
  if (reads_.empty()) {
    read_callback_ = std::move(callback);
    return;
  }

  std::vector<uint8_t> bytes = std::move(reads_.front());
  reads_.pop();
  std::move(callback)(std::move(bytes));
}

void NearbyConnectionImpl::Write(std::vector<uint8_t> bytes) {
  MutexLock lock(&mutex_);
  Payload payload(bytes);
  nearby_connections_manager_->Send(
      endpoint_id_, std::make_unique<Payload>(payload),
      /*listener=*/
      std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>());
}

void NearbyConnectionImpl::Close() {
  MutexLock lock(&mutex_);
  // As [this] therefore endpoint_id_ will be destroyed in Disconnect, make a
  // copy of [endpoint_id] as the parameter is a const ref.
  nearby_connections_manager_->Disconnect(endpoint_id_);
}

void NearbyConnectionImpl::SetDisconnectionListener(
    std::function<void()> listener) {
  MutexLock lock(&mutex_);
  disconnect_listener_ = std::move(listener);
}

void NearbyConnectionImpl::WriteMessage(std::vector<uint8_t> bytes) {
  MutexLock lock(&mutex_);
  if (read_callback_) {
    auto callback = std::move(read_callback_);
    read_callback_ = nullptr;
    callback(std::move(bytes));
    return;
  }

  reads_.push(std::move(bytes));
}

}  // namespace sharing
}  // namespace nearby
