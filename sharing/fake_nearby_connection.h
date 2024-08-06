// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTION_H_
#define THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTION_H_

#include <stdint.h>

#include <functional>
#include <optional>
#include <queue>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/task_runner.h"
#include "sharing/nearby_connection.h"

namespace nearby {
namespace sharing {

class FakeNearbyConnection : public NearbyConnection {
 public:
  explicit FakeNearbyConnection(TaskRunner* task_runner = nullptr);
  ~FakeNearbyConnection() override;

  // NearbyConnection:
  void Read(std::function<void(std::optional<std::vector<uint8_t>> bytes)>
                callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void SetDisconnectionListener(std::function<void()> listener) override;

  void AppendReadableData(std::vector<uint8_t> bytes)
      ABSL_LOCKS_EXCLUDED(read_mutex_);
  std::vector<uint8_t> GetWrittenData();

  bool IsClosed();
  bool has_read_callback_been_run() {
    absl::MutexLock lock(&read_mutex_);
    return has_read_callback_been_run_;
  }

 private:
  void MaybeRunCallback() ABSL_LOCKS_EXCLUDED(read_mutex_);

  bool closed_ = false;

  TaskRunner* const task_runner_;
  absl::Mutex read_mutex_;
  bool has_read_callback_been_run_ ABSL_GUARDED_BY(read_mutex_) = false;
  std::function<void(std::optional<std::vector<uint8_t>> bytes)> callback_
      ABSL_GUARDED_BY(read_mutex_);
  std::queue<std::vector<uint8_t>> read_data_ ABSL_GUARDED_BY(read_mutex_);
  absl::Mutex write_mutex_;
  std::queue<std::vector<uint8_t>> write_data_ ABSL_GUARDED_BY(write_mutex_);
  absl::Mutex disconnect_mutex_;
  std::function<void()> disconnect_listener_ ABSL_GUARDED_BY(disconnect_mutex_);
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTION_H_
