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
#include <queue>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "internal/platform/device_info.h"
#include "internal/platform/mutex.h"
#include "sharing/nearby_connection.h"

namespace nearby {
namespace sharing {

class NearbyConnectionsManager;

class NearbyConnectionImpl : public NearbyConnection {
 public:
  NearbyConnectionImpl(nearby::DeviceInfo& device_info,
                       NearbyConnectionsManager* nearby_connections_manager,
                       absl::string_view endpoint_id);
  ~NearbyConnectionImpl() override;

  // NearbyConnection:
  void Read(ReadCallback callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void SetDisconnectionListener(std::function<void()> listener) override;

  // Add bytes to the read queue, notifying ReadCallback.
  void WriteMessage(std::vector<uint8_t> bytes);

 private:
  nearby::DeviceInfo& device_info_;
  NearbyConnectionsManager* const nearby_connections_manager_;
  std::string endpoint_id_;

  RecursiveMutex mutex_;
  ReadCallback read_callback_ ABSL_GUARDED_BY(mutex_) = nullptr;
  std::function<void()> disconnect_listener_ ABSL_GUARDED_BY(mutex_);

  // A read queue. The data that we've read from the remote device ends up here
  // until Read() is called to dequeue it.
  std::queue<std::vector<uint8_t>> reads_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_IMPL_H_
