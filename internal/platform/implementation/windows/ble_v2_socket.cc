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

#include "internal/platform/implementation/windows/ble_v2_socket.h"

#include <cstddef>
#include <memory>

#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

InputStream& BleV2Socket::GetInputStream() { return input_stream_; }

OutputStream& BleV2Socket::GetOutputStream() { return output_stream_; }

Exception BleV2Socket::Close() { return {Exception::kSuccess}; }

api::ble_v2::BlePeripheral* BleV2Socket::GetRemotePeripheral() {
  return ble_peripheral_;
}

bool BleV2Socket::Connect(api::ble_v2::BlePeripheral* ble_peripheral) {
  // TODO(b/271031645): implement BLE socket using weave
  NEARBY_LOGS(VERBOSE) << __func__ << ": Connect to BLE peripheral="
                       << ble_peripheral->GetAddress();
  return false;
}

ExceptionOr<ByteArray> BleV2Socket::BleInputStream::Read(std::int64_t size) {
  // TODO(b/271031645): implement BLE socket using weave
  NEARBY_LOGS(VERBOSE) << __func__ << ": Read data size=" << size;
  return ExceptionOr<ByteArray>(Exception::kIo);
}

Exception BleV2Socket::BleInputStream::Close() {
  // TODO(b/271031645): implement BLE socket using weave
  NEARBY_LOGS(VERBOSE) << __func__ << ": Close BLE input stream.";
  return {Exception::kSuccess};
}

Exception BleV2Socket::BleOutputStream::Write(const ByteArray& data) {
  // TODO(b/271031645): implement BLE socket using weave
  NEARBY_LOGS(VERBOSE) << __func__ << ": Write data size=" << data.size();
  return {Exception::kIo};
}

Exception BleV2Socket::BleOutputStream::Flush() {
  // TODO(b/271031645): implement BLE socket using weave
  NEARBY_LOGS(INFO) << __func__ << ": Flush is called.";
  return {Exception::kSuccess};
}

Exception BleV2Socket::BleOutputStream::Close() {
  // TODO(b/271031645): implement BLE socket using weave
  NEARBY_LOGS(INFO) << __func__ << ": close is called.";
  return {Exception::kSuccess};
}

}  // namespace windows
}  // namespace nearby
