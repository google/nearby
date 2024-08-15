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

#include "sharing/fake_nearby_connection.h"

#include <stdint.h>

#include <functional>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {
FakeNearbyConnection::FakeNearbyConnection(TaskRunner* task_runner)
    : task_runner_(task_runner) {}
FakeNearbyConnection::~FakeNearbyConnection() = default;

void FakeNearbyConnection::Read(
    std::function<void(std::optional<std::vector<uint8_t>> bytes)> callback) {
  NL_DCHECK(!closed_);
  {
    absl::MutexLock lock(&read_mutex_);
    callback_ = std::move(callback);
  }
  MaybeRunCallback();
}

void FakeNearbyConnection::Write(std::vector<uint8_t> bytes) {
  NL_DCHECK(!closed_);
  absl::MutexLock lock(&write_mutex_);
  write_data_.push(std::move(bytes));
}

void FakeNearbyConnection::Close() {
  NL_DCHECK(!closed_);
  closed_ = true;

  {
    if (task_runner_) {
      task_runner_->PostTask([this]() {
        absl::MutexLock lock(&disconnect_mutex_);
        if (disconnect_listener_) {
          std::move(disconnect_listener_)();
        }
      });
      return;
    }
    absl::MutexLock lock(&disconnect_mutex_);
    if (disconnect_listener_) {
      std::move(disconnect_listener_)();
    }
  }

  absl::MutexLock lock(&read_mutex_);
  if (callback_) {
    has_read_callback_been_run_ = true;
    auto callback = std::move(callback_);
    callback_ = nullptr;
    callback(std::nullopt);
  }
}

void FakeNearbyConnection::SetDisconnectionListener(
    std::function<void()> listener) {
  NL_DCHECK(!closed_);
  absl::MutexLock lock(&disconnect_mutex_);
  disconnect_listener_ = std::move(listener);
}

void FakeNearbyConnection::AppendReadableData(std::vector<uint8_t> bytes) {
  NL_DCHECK(!closed_);
  if (task_runner_) {
    task_runner_->PostTask([this, bytes = std::move(bytes)]() {
      {
        absl::MutexLock lock(&read_mutex_);
        read_data_.push(std::move(bytes));
      }
      MaybeRunCallback();
    });
    return;
  }
  {
    absl::MutexLock lock(&read_mutex_);
    read_data_.push(std::move(bytes));
  }
  MaybeRunCallback();
}

std::vector<uint8_t> FakeNearbyConnection::GetWrittenData() {
  absl::MutexLock lock(&write_mutex_);
  if (write_data_.empty()) return {};

  std::vector<uint8_t> bytes = std::move(write_data_.front());
  write_data_.pop();
  return bytes;
}

bool FakeNearbyConnection::IsClosed() { return closed_; }

void FakeNearbyConnection::MaybeRunCallback() {
  NL_DCHECK(!closed_);
  std::vector<uint8_t> item;
  std::function<void(std::optional<std::vector<uint8_t>> bytes)> callback;
  {
    absl::MutexLock lock(&read_mutex_);
    if (!callback_ || read_data_.empty()) return;
    item = std::move(read_data_.front());
    read_data_.pop();
    callback = std::move(callback_);
    callback_ = nullptr;
    has_read_callback_been_run_ = true;
  }
  callback(std::move(item));
}

}  // namespace sharing
}  // namespace nearby
