// Copyright 2020 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_BLE_H_
#define PLATFORM_IMPL_WINDOWS_BLE_H_

#include "platform/api/ble.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"
#include "platform/base/output_stream.h"

namespace location {
namespace nearby {
namespace windows {

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE device to connect to its GATT server.
class BlePeripheral : public api::BlePeripheral {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BlePeripheral() override = default;

  // TODO(b/184975123): replace with real implementation.
  std::string GetName() const override { return std::string{""}; }
  // TODO(b/184975123): replace with real implementation.
  ByteArray GetAdvertisementBytes(
      const std::string& service_id) const override {
    return ByteArray{};
  }
};

class BleSocket : public api::BleSocket {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BleSocket() override;

  // Returns the InputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  // TODO(b/184975123): replace with real implementation.
  InputStream& GetInputStream() override { return fake_input_stream_; }

  // Returns the OutputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  // TODO(b/184975123): replace with real implementation.
  OutputStream& GetOutputStream() override { return fake_output_stream_; }

  // Conforms to the same contract as
  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#close().
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // TODO(b/184975123): replace with real implementation.
  Exception Close() override { return Exception{}; }

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  // TODO(b/184975123): replace with real implementation.
  BlePeripheral* GetRemotePeripheral() override { return nullptr; }

  // Unhooked InputStream & OutputStream for empty implementation.
  // TODO(b/184975123): replace with real implementation.
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
};

// Container of operations that can be performed over the BLE medium.
class BleMedium : public api::BleMedium {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BleMedium() override = default;

  // TODO(b/184975123): replace with real implementation.
  bool StartAdvertising(
      const std::string& service_id, const ByteArray& advertisement_bytes,
      const std::string& fast_advertisement_service_uuid) override {
    return false;
  }
  // TODO(b/184975123): replace with real implementation.
  bool StopAdvertising(const std::string& service_id) override { return false; }

  // Returns true once the BLE scan has been initiated.
  // TODO(b/184975123): replace with real implementation.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback) override {
    return false;
  }

  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  // TODO(b/184975123): replace with real implementation.
  bool StopScanning(const std::string& service_id) override { return false; }

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  // TODO(b/184975123): replace with real implementation.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override {
    return false;
  }
  // TODO(b/184975123): replace with real implementation.
  bool StopAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  // Connects to a BLE peripheral.
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<api::BleSocket> Connect(
      api::BlePeripheral& peripheral, const std::string& service_id,
      CancellationFlag* cancellation_flag) override {
    return nullptr;
  }
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_BLE_H_
