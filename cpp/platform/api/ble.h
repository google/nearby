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

#include "platform/api/bluetooth_classic.h"
#include "platform/api/input_stream.h"
#include "platform/api/output_stream.h"
#include "platform/byte_array.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE device to connect to its GATT server.
class BLEPeripheral {
 public:
  virtual ~BLEPeripheral() {}

  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the corresponding BLEPeripheral object is destroyed.
  virtual Ptr<BluetoothDevice> getBluetoothDevice() = 0;
};

class BLESocket {
 public:
  virtual ~BLESocket() {}

  // Returns the InputStream of the BLESocket, or a null Ptr<InputStream>
  // on error.
  //
  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the BLESocket object is destroyed.
  virtual Ptr<InputStream> getInputStream() = 0;

  // Returns the OutputStream of the BLESocket, or a null
  // Ptr<OutputStream> on error.
  //
  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the BLESocket object is destroyed.
  virtual Ptr<OutputStream> getOutputStream() = 0;

  // Conforms to the same contract as
  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#close().
  //
  // Returns Exception::IO on error, Exception::NONE otherwise.
  virtual Exception::Value close() = 0;

  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the BLESocket object is destroyed.
  virtual Ptr<BLEPeripheral> getRemotePeripheral() = 0;
};

// Container of operations that can be performed over the BLE medium.
class BLEMedium {
 public:
  virtual ~BLEMedium() {}

  // Takes ownership of (and is responsible for destroying) the passed-in
  // 'advertisement'.
  virtual bool startAdvertising(const std::string& service_id,
                                ConstPtr<ByteArray> advertisement) = 0;
  virtual void stopAdvertising(const std::string& service_id) = 0;

  class DiscoveredPeripheralCallback {
   public:
    virtual ~DiscoveredPeripheralCallback() {}

    // The Ptrs provided in these callback methods will be owned (and
    // destroyed) by the recipient of the callback methods (i.e. the creator of
    // the concrete DiscoveredPeripheralCallback object).
    virtual void onPeripheralDiscovered(Ptr<BLEPeripheral> ble_peripheral,
                                        const std::string& service_id,
                                        ConstPtr<ByteArray> advertisement) = 0;
    virtual void onPeripheralLost(Ptr<BLEPeripheral> ble_peripheral,
                                  const std::string& service_id) = 0;
  };

  // Returns true once the BLE scan has been initiated.
  //
  // Does not take ownership of the passed-in discovered_peripheral_callback --
  // destroying that is up to the caller.
  virtual bool startScanning(
      const std::string& service_id,
      Ptr<DiscoveredPeripheralCallback> discovered_peripheral_callback) = 0;
  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to startScanning() for service_id.
  //
  // Does not need to bother with destroying the DiscoveredPeripheralCallback
  // passed in to startScanning() -- that's the job of the caller.
  virtual void stopScanning(const std::string& service_id) = 0;

  // Callback that is invoked when a new connection is accepted.
  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() {}

    // The Ptr provided in this callback method will be owned (and
    // destroyed) by the recipient of the callback methods (i.e. the creator of
    // the concrete AcceptedConnectionCallback object).
    virtual void onConnectionAccepted(Ptr<BLESocket> socket,
                                      const std::string& service_id) = 0;
  };

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  //
  // Does not take ownership of the passed-in accepted_connection_callback --
  // destroying that is up to the caller.
  virtual bool startAcceptingConnections(
      const std::string& service_id,
      Ptr<AcceptedConnectionCallback> accepted_connection_callback) = 0;
  virtual void stopAcceptingConnections(const std::string& service_id) = 0;

  // The returned Ptr will be owned (and destroyed) by the caller. Returns
  // a null Ptr<BleSocket> on error.
  virtual Ptr<BLESocket> connect(Ptr<BLEPeripheral> ble_peripheral,
                                 const std::string& service_id) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_BLE_H_
