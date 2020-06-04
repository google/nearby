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

#ifndef PLATFORM_V2_API_BLE_H_
#define PLATFORM_V2_API_BLE_H_

#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/base/output_stream.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace api {

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE device to connect to its GATT server.
class BlePeripheral {
 public:
  virtual ~BlePeripheral() {}

  // The returned reference lifetime matches BlePeripheral object.
  virtual BluetoothDevice& GetBluetoothDevice() = 0;
};

class BleSocket {
 public:
  virtual ~BleSocket() {}

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

  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  virtual BlePeripheral& GetRemotePeripheral() = 0;
};

// Container of operations that can be performed over the BLE medium.
class BleMedium {
 public:
  virtual ~BleMedium() {}

  virtual bool StartAdvertising(absl::string_view service_id,
                                const ByteArray& advertisement) = 0;
  virtual void StopAdvertising(absl::string_view service_id) = 0;

  class DiscoveredPeripheralCallback {
   public:
    virtual ~DiscoveredPeripheralCallback() {}

    // The BlePeripheral* is not owned by callbacks.
    // It is passed to give access to its non-const methods.
    // It is guaranteed to be valid for the duration of call.
    virtual void OnPeripheralDiscovered(BlePeripheral* ble_peripheral,
                                        absl::string_view service_id,
                                        const ByteArray& advertisement) = 0;
    virtual void OnPeripheralLost(BlePeripheral* ble_peripheral,
                                  absl::string_view service_id) = 0;
  };

  // Returns true once the BLE scan has been initiated.
  virtual bool StartScanning(
      absl::string_view service_id,
      const DiscoveredPeripheralCallback& discovered_peripheral_callback) = 0;

  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  virtual void StopScanning(absl::string_view service_id) = 0;

  // Callback that is invoked when a new connection is accepted.
  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() {}

    virtual void OnConnectionAccepted(std::unique_ptr<BleSocket> socket,
                                      absl::string_view service_id) = 0;
  };

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  virtual bool StartAcceptingConnections(
      absl::string_view service_id,
      const AcceptedConnectionCallback& accepted_connection_callback) = 0;
  virtual void StopAcceptingConnections(const std::string& service_id) = 0;

  // BlePeripheral* is not owned by this call;
  // it must remain valid for the duration of a call.
  virtual std::unique_ptr<BleSocket> Connect(BlePeripheral* ble_peripheral,
                                             absl::string_view service_id) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_BLE_H_
