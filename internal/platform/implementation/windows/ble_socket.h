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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_SOCKET_H_

#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/windows/ble.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {

// TODO(b/269152309): Implement BLE Weave Socket
// This is just a fake stub to appease the BLE Medium abstraction. Windows
// does not support BLE Weave sockets currently. BLE is only used in the
// pre-connection phase handshake (advertising & discovering), connection is
// delegated to Bluetooth Classic Medium to establish RFCOMM socket.
class BleSocket : public api::BleSocket {
 public:
  ~BleSocket() override;

  // Returns the InputStream of this connected BleSocket.
  InputStream& GetInputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the OutputStream of this connected BleSocket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if socket is closed.
  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  BlePeripheral* GetRemotePeripheral() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Unhooked InputStream & OutputStream for empty implementation.
 private:
  class FakeInputStream : public InputStream {
    ~FakeInputStream() override = default;
    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return ExceptionOr<ByteArray>(Exception::kFailed);
    }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  class FakeOutputStream : public OutputStream {
    ~FakeOutputStream() override = default;

    Exception Write(const ByteArray& data) override {
      return {.value = Exception::kFailed};
    }
    Exception Flush() override { return {.value = Exception::kFailed}; }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
  mutable absl::Mutex mutex_;
  BlePeripheral* peripheral_;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_SOCKET_H_
