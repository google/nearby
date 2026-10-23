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

#include "internal/platform/implementation/windows/ble_socket.h"

#include <cstdint>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {

InputStream& BleSocket::GetInputStream() { return input_stream_; }

OutputStream& BleSocket::GetOutputStream() { return output_stream_; }

Exception BleSocket::Close() { return {Exception::kSuccess}; }

bool BleSocket::Connect() {
  return false;
}

ExceptionOr<ByteArray> BleSocket::BleInputStream::Read(std::int64_t size) {
  return ExceptionOr<ByteArray>(Exception::kIo);
}

Exception BleSocket::BleInputStream::Close() {
  return {Exception::kSuccess};
}

Exception BleSocket::BleOutputStream::Write(absl::string_view data) {
  return {Exception::kIo};
}

Exception BleSocket::BleOutputStream::Flush() {
  return {Exception::kSuccess};
}

Exception BleSocket::BleOutputStream::Close() {
  return {Exception::kSuccess};
}

}  // namespace windows
}  // namespace nearby
