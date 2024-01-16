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
#include <queue>
#include <vector>

#include "sharing/nearby_connection.h"

namespace nearby {
namespace sharing {

class FakeNearbyConnection : public NearbyConnection {
 public:
  FakeNearbyConnection();
  ~FakeNearbyConnection() override;

  // NearbyConnection:
  void Read(ReadCallback callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void SetDisconnectionListener(std::function<void()> listener) override;

  void AppendReadableData(std::vector<uint8_t> bytes);
  std::vector<uint8_t> GetWrittenData();

  bool IsClosed();
  bool has_read_callback_been_run() { return has_read_callback_been_run_; }

 private:
  void MaybeRunCallback();

  bool closed_ = false;
  bool has_read_callback_been_run_ = false;

  ReadCallback callback_;
  std::queue<std::vector<uint8_t>> read_data_;
  std::queue<std::vector<uint8_t>> write_data_;
  std::function<void()> disconnect_listener_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTION_H_
