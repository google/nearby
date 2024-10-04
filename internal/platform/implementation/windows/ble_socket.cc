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

#include "internal/platform/implementation/windows/ble_socket.h"

#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/ble_peripheral.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {

InputStream& BleSocket::GetInputStream() {
  return fake_input_stream_;
}

OutputStream& BleSocket::GetOutputStream() {
  return fake_output_stream_;
}

bool BleSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

Exception BleSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return {Exception::kSuccess};
}

BlePeripheral* BleSocket::GetRemotePeripheral() {
  absl::MutexLock lock(&mutex_);
  return peripheral_;
}

}  // namespace windows
}  // namespace nearby
