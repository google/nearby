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

#ifndef PLATFORM_API_BLE_H_
#define PLATFORM_API_BLE_H_

#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE device to connect to its GATT server.
class BlePeripheral {
 public:
  virtual ~BlePeripheral() = default;

  virtual std::string GetName() const = 0;

  virtual ByteArray GetAdvertisementBytes(
      const std::string& service_id) const = 0;
};

class BleSocket {
 public:
  virtual ~BleSocket() = default;

  // Returns the InputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  virtual OutputStream& GetOutputStream() = 0;

  // Conforms to the same contract as
  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#close().
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  virtual BlePeripheral* GetRemotePeripheral() = 0;
};

// Container of operations that can be performed over the BLE medium.
class BleMedium {
 public:
  virtual ~BleMedium() = default;

  virtual bool StartAdvertising(
      const std::string& service_id, const ByteArray& advertisement_bytes,
      const std::string& fast_advertisement_service_uuid) = 0;
  virtual bool StopAdvertising(const std::string& service_id) = 0;

  // Callback that is invoked when a discovered peripheral is found or lost.
  struct DiscoveredPeripheralCallback {
    absl::AnyInvocable<void(BlePeripheral& peripheral,
                            const std::string& service_id,
                            bool fast_advertisement)>
        peripheral_discovered_cb =
            DefaultCallback<BlePeripheral&, const std::string&, bool>();
    absl::AnyInvocable<void(BlePeripheral& peripheral,
                            const std::string& service_id)>
        peripheral_lost_cb =
            DefaultCallback<BlePeripheral&, const std::string&>();
  };

  // Returns true once the BLE scan has been initiated.
  virtual bool StartScanning(const std::string& service_id,
                             const std::string& fast_advertisement_service_uuid,
                             DiscoveredPeripheralCallback callback) = 0;

  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  virtual bool StopScanning(const std::string& service_id) = 0;

  // Callback that is invoked when a new connection is accepted.
  struct AcceptedConnectionCallback {
    absl::AnyInvocable<void(BleSocket& socket, const std::string& service_id)>
        accepted_cb = DefaultCallback<BleSocket&, const std::string&>();
  };

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  virtual bool StartAcceptingConnections(
      const std::string& service_id, AcceptedConnectionCallback callback) = 0;
  virtual bool StopAcceptingConnections(const std::string& service_id) = 0;

  // Connects to a BLE peripheral.
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<BleSocket> Connect(
      BlePeripheral& peripheral, const std::string& service_id,
      CancellationFlag* cancellation_flag) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_BLE_H_
