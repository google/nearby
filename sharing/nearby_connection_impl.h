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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/device_info.h"
#include "sharing/nearby_connection.h"

namespace nearby::sharing {

class NearbyConnectionsManager;

class NearbyConnectionImpl : public NearbyConnection {
 public:
  explicit NearbyConnectionImpl(nearby::DeviceInfo& device_info);
  ~NearbyConnectionImpl() override;

  // NearbyConnection:
  void Read(
      std::function<void(std::optional<std::vector<uint8_t>> bytes)> callback)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  void SetDisconnectionListener(std::function<void()> listener)
      ABSL_LOCKS_EXCLUDED(mutex_) override;

  // Add bytes to the read queue, notifying ReadCallback.
  void WriteMessage(std::vector<uint8_t> bytes) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  nearby::DeviceInfo& device_info_;

  absl::Mutex mutex_;
  std::function<void(std::optional<std::vector<uint8_t>> bytes)> read_callback_
      ABSL_GUARDED_BY(mutex_) = nullptr;
  std::function<void()> disconnect_listener_ ABSL_GUARDED_BY(mutex_);

  // A read queue. The data that we've read from the remote device ends up here
  // until Read() is called to dequeue it.
  std::queue<std::vector<uint8_t>> reads_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_
