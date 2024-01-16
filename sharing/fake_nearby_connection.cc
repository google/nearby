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

#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"

namespace nearby {
namespace sharing {
FakeNearbyConnection::FakeNearbyConnection() = default;
FakeNearbyConnection::~FakeNearbyConnection() = default;

void FakeNearbyConnection::Read(ReadCallback callback) {
  NL_DCHECK(!closed_);
  callback_ = std::move(callback);
  MaybeRunCallback();
}

void FakeNearbyConnection::Write(std::vector<uint8_t> bytes) {
  NL_DCHECK(!closed_);
  write_data_.push(std::move(bytes));
}

void FakeNearbyConnection::Close() {
  NL_DCHECK(!closed_);
  closed_ = true;

  if (disconnect_listener_) {
    std::move(disconnect_listener_)();
  }

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
  disconnect_listener_ = std::move(listener);
}

void FakeNearbyConnection::AppendReadableData(std::vector<uint8_t> bytes) {
  NL_DCHECK(!closed_);
  read_data_.push(std::move(bytes));
  MaybeRunCallback();
}

std::vector<uint8_t> FakeNearbyConnection::GetWrittenData() {
  if (write_data_.empty()) return {};

  std::vector<uint8_t> bytes = std::move(write_data_.front());
  write_data_.pop();
  return bytes;
}

bool FakeNearbyConnection::IsClosed() { return closed_; }

void FakeNearbyConnection::MaybeRunCallback() {
  NL_DCHECK(!closed_);
  if (!callback_ || read_data_.empty()) return;
  auto item = std::move(read_data_.front());
  read_data_.pop();
  has_read_callback_been_run_ = true;
  auto callback = std::move(callback_);
  callback_ = nullptr;
  callback(std::move(item));
}

}  // namespace sharing
}  // namespace nearby
