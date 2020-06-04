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

#ifndef PLATFORM_API_BLE_V2_H_
#define PLATFORM_API_BLE_V2_H_

#include <cstdint>
#include <limits>
#include <map>
#include <set>

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/bluetooth/le/AdvertiseData
//
// Bundle of data found in a BLE advertisement.
//
// All service UUIDs will conform to the 16-bit Bluetooth base UUID,
// 0000xxxx-0000-1000-8000-00805F9B34FB. This makes it possible to store two
// byte service UUIDs in the advertisement.
struct BLEAdvertisementData {
  typedef std::int8_t TXPowerLevel;

  static constexpr TXPowerLevel UNSPECIFIED_TX_POWER_LEVEL =
      std::numeric_limits<TXPowerLevel>::min();

  bool is_connectable;
  // When set to UNSPECIFIED_TX_POWER_LEVEL, TX power should not be included in
  // the advertisement data.
  TXPowerLevel tx_power_level;
  // When set to an empty string, local name should not be included in the
  // advertisement data.
  std::string local_name;
  // When set to an empty vector, the set of 16-bit service class UUIDs should
  // not be included in the advertisement data.
  std::set<std::string> service_uuids;
  // Maps service UUIDs to their service data.
  // Ownership of the map values is tied to ownership of BLEAdvertisementData.
  std::map<std::string, ConstPtr<ByteArray>> service_data;
};

// Opaque wrapper over a BLE peripheral. Must be able to uniquely identify a
// peripheral so that we can connect to its GATT server.
//
// BLEPeripheralV2 should always be created as a RefCountedPtr because ownership
// is shared between the per-platform implementation and the internals of Nearby
// Connections.
class BLEPeripheralV2 {
 public:
  virtual ~BLEPeripheralV2() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice#getAddress()
  //
  // This should be the MAC address when possible. If the implementation is
  // unable to retrieve that, any unique identifier should suffice.
  virtual std::string getId() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic
//
// Representation of a GATT characteristic.
//
// GATTCharacteristics are RefCounted so that ownership can be shared between
// the per-platform implementation and C++ internals. All GATTCharacteristics
// should be created with MakeRefCountedPtr().
class GATTCharacteristic {
 public:
  virtual ~GATTCharacteristic() {}

  // Possible permissions of a GATT characteristic.
  struct Permission {
    enum Value {
      UNKNOWN = 0,
      READ = 1,
      WRITE = 2,
    };
  };

  // Possible properties of a GATT characteristic.
  struct Property {
    enum Value {
      UNKNOWN = 0,
      READ = 1,
      WRITE = 2,
      INDICATE = 3,
    };
  };

  // Returns the UUID of this characteristic.
  virtual std::string getUUID() = 0;

  // Returns the UUID of the containing GATT service.
  virtual std::string getServiceUUID() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGatt
//
// Representation of a client GATT connection to a remote GATT server.
class ClientGATTConnection {
 public:
  virtual ~ClientGATTConnection() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#getDevice()
  //
  // Retrieves the BLE peripheral that this connection is tied to.
  virtual Ptr<BLEPeripheralV2> getPeripheral() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#discoverServices()
  //
  // Discovers all available services and characteristics on this connection.
  // Returns whether or not discovery finished successfully.
  //
  // This function should block until discovery has finished.
  virtual bool discoverServices() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#getService(java.util.UUID)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattService.html#getCharacteristic(java.util.UUID)
  //
  // Retrieves a GATT characteristic. A null Ptr is returned upon error.
  //
  // discoverServices() should be called before this method to fetch all
  // available services and characteristics first.
  //
  // It is okay for duplicate services to exist, as long as the specified
  // characteristic UUID is unique among all services of the same UUID.
  virtual Ptr<GATTCharacteristic> getCharacteristic(
      const std::string& service_uuid,
      const std::string& characteristic_uuid) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#readCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#getValue()
  //
  // Reads a GATT characteristic. A null ConstPtr is returned upon error.
  virtual ConstPtr<ByteArray> readCharacteristic(
      Ptr<GATTCharacteristic> characteristic) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#writeCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  //
  // Sends a remote characteristic write request to the server and returns
  // whether or not it was successful.
  virtual bool writeCharacteristic(Ptr<GATTCharacteristic> characteristic,
                                   ConstPtr<ByteArray> value) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#disconnect()
  //
  // Disconnects a GATT connection.
  virtual void disconnect() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattServer
//
// Representation of a server GATT connection to a remote GATT client.
class ServerGATTConnection {
 public:
  virtual ~ServerGATTConnection() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattServer.html#notifyCharacteristicChanged(android.bluetooth.BluetoothDevice,%20android.bluetooth.BluetoothGattCharacteristic,%20boolean)
  //
  // Sends a notification (via indication) to the client that a characteristic
  // has changed with the given value. Returns whether or not it was successful.
  //
  // The value sent does not have to reflect the locally stored characteristic
  // value. To update the local value, call GATTServer::updateCharacteristic.
  virtual bool sendCharacteristic(Ptr<GATTCharacteristic> characteristic,
                                  ConstPtr<ByteArray> value) = 0;
};

// Callback for asynchronous events on the client side of a GATT connection.
class ClientGATTConnectionLifecycleCallback {
 public:
  virtual ~ClientGATTConnectionLifecycleCallback() {}

  // Called when the client is disconnected from the GATT server.
  virtual void onDisconnected(Ptr<ClientGATTConnection> connection) = 0;
};

// Callback for asynchronous events on the server side of a GATT connection.
class ServerGATTConnectionLifecycleCallback {
 public:
  virtual ~ServerGATTConnectionLifecycleCallback() {}

  // Called when a remote peripheral connected to us and subscribed to one of
  // our characteristics.
  virtual void onCharacteristicSubscription(
      Ptr<ServerGATTConnection> connection,
      Ptr<GATTCharacteristic> characteristic) = 0;

  // Called when a remote peripheral unsubscribed from one of our
  // characteristics.
  virtual void onCharacteristicUnsubscription(
      Ptr<ServerGATTConnection> connection,
      Ptr<GATTCharacteristic> characteristic) = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattServer
//
// Representation of a BLE GATT server.
class GATTServer {
 public:
  virtual ~GATTServer() {}

  // Creates a characteristic and adds it to the GATT server under the given
  // characteristic and service UUIDs. Returns a null Ptr upon error.
  //
  // Characteristics of the same service UUID should be put under one
  // service rather than many services with the same UUID.
  //
  // If the INDICATE property is included, the characteristic should include the
  // official Bluetooth Client Characteristic Configuration descriptor with UUID
  // 0x2902 and a WRITE permission. This allows remote clients to write to this
  // descriptor and subscribe for characteristic changes. For more information
  // about this descriptor, please go to:
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  virtual Ptr<GATTCharacteristic> createCharacteristic(
      const std::string& service_uuid,
      const std::string& characteristic_uuid,
      const std::set<GATTCharacteristic::Permission::Value>& permissions,
      const std::set<GATTCharacteristic::Property::Value>& properties) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  //
  // Locally updates the value of a characteristic and returns whether or not it
  // was successful.
  // Takes ownership of (and is responsible for destroying) the passed-in
  // 'value'.
  virtual bool updateCharacteristic(Ptr<GATTCharacteristic> characteristic,
                                    ConstPtr<ByteArray> value) = 0;

  // Stops a GATT server.
  virtual void stop() = 0;
};

// A BLE socket representation.
class BLESocketV0 {
 public:
  virtual ~BLESocketV0() {}

  // Returns the remote BLE peripheral tied to this socket.
  virtual Ptr<BLEPeripheralV2> getRemotePeripheral() = 0;

  // Writes a message on the socket and blocks until finished. Returns
  // Exception::IO upon error, and Exception::NONE otherwise.
  virtual Exception::Value write(ConstPtr<ByteArray> message) = 0;

  // Closes the socket and blocks until finished. Returns Exception::IO upon
  // error, and Exception::NONE otherwise.
  virtual Exception::Value close() = 0;
};

// Callback for asynchronous events on a BLESocketV0 object.
class BLESocketLifecycleCallback {
 public:
  virtual ~BLESocketLifecycleCallback() {}

  // Called when a message arrives on a socket.
  virtual void onMessageReceived(Ptr<BLESocketV0> socket,
                                 ConstPtr<ByteArray> message) = 0;

  // Called when a socket gets disconnected.
  virtual void onDisconnected(Ptr<BLESocketV0> socket) = 0;
};

// Callback for asynchronous events on the server side of a BLESocketV0 object.
class ServerBLESocketLifecycleCallback : public BLESocketLifecycleCallback {
 public:
  ~ServerBLESocketLifecycleCallback() override {}

  // Called when a new incoming socket has been established.
  virtual void onSocketEstablished(Ptr<BLESocketV0> socket) = 0;
};

// The main BLE medium used inside of Nearby. This serves as the entry point for
// all BLE and GATT related operations.
class BLEMediumV2 {
 public:
  virtual ~BLEMediumV2() {}

  typedef std::uint32_t MTU;

  // Coarse representation of power settings throughout all BLE operations.
  struct PowerMode {
    enum Value {
      UNKNOWN = 0,
      LOW = 1,
      HIGH = 2,
    };
  };

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeAdvertiser.html#startAdvertising(android.bluetooth.le.AdvertiseSettings,%20android.bluetooth.le.AdvertiseData,%20android.bluetooth.le.AdvertiseData,%20android.bluetooth.le.AdvertiseCallback)
  //
  // Starts BLE advertising and returns whether or not it was successful.
  //
  // Power mode should be interpreted in the following way:
  //   LOW:
  //     - Advertising interval = ~1000ms
  //     - TX power = low
  //   HIGH:
  //     - Advertising interval = ~100ms
  //     - TX power = high
  virtual bool startAdvertising(
      ConstPtr<BLEAdvertisementData> advertisement_data,
      ConstPtr<BLEAdvertisementData> scan_response,
      PowerMode::Value power_mode) = 0;

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeAdvertiser.html#stopAdvertising(android.bluetooth.le.AdvertiseCallback)
  //
  // Stops advertising.
  virtual void stopAdvertising() = 0;

  // https://developer.android.com/reference/android/bluetooth/le/ScanCallback
  //
  // Callback for BLE scan results.
  class ScanCallback {
   public:
    virtual ~ScanCallback() {}

    // https://developer.android.com/reference/android/bluetooth/le/ScanCallback.html#onScanResult(int,%20android.bluetooth.le.ScanResult)
    //
    // Called when a BLE advertisement is discovered.
    //
    // The passed in advertisement_data is the merged combination of both
    // advertisement data and scan response.
    //
    // Every discovery of an advertisement should be reported, even if the
    // advertisement was discovered before.
    //
    // Ownership of the BLEAdvertisementData transfers to the caller at this
    // point.
    virtual void onAdvertisementFound(
        Ptr<BLEPeripheralV2> peripheral,
        ConstPtr<BLEAdvertisementData> advertisement_data) = 0;
  };

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeScanner.html#startScan(java.util.List%3Candroid.bluetooth.le.ScanFilter%3E,%20android.bluetooth.le.ScanSettings,%20android.bluetooth.le.ScanCallback)
  //
  // Starts scanning and returns whether or not it was successful.
  //
  // Power mode should be interpreted in the following way:
  //   LOW:
  //     - Scan window = ~512ms
  //     - Scan interval = ~5120ms
  //   HIGH:
  //     - Scan window = ~4096ms
  //     - Scan interval = ~4096ms
  virtual bool startScanning(const std::set<std::string>& service_uuids,
                             PowerMode::Value power_mode,
                             Ptr<ScanCallback> scan_callback) = 0;

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeScanner.html#stopScan(android.bluetooth.le.ScanCallback)
  //
  // Stops scanning.
  virtual void stopScanning() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothManager#openGattServer(android.content.Context,%20android.bluetooth.BluetoothGattServerCallback)
  //
  // Starts a GATT server. Returns a null Ptr upon error.
  virtual Ptr<GATTServer> startGATTServer(
      Ptr<ServerGATTConnectionLifecycleCallback>
          connection_lifecycle_callback) = 0;

  // Starts listening for incoming BLE sockets and returns false upon error.
  virtual bool startListeningForIncomingBLESockets(
      Ptr<ServerBLESocketLifecycleCallback> socket_lifecycle_callback) = 0;

  // Stops listening for incoming BLE sockets.
  virtual void stopListeningForIncomingBLESockets() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#connectGatt(android.content.Context,%20boolean,%20android.bluetooth.BluetoothGattCallback)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#requestConnectionPriority(int)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#requestMtu(int)
  //
  // Connects to a GATT server and negotiates the specified connection
  // parameters. Returns a null Ptr upon error.
  //
  // Both connection interval and MTU can be negotiated on a best-effort basis.
  //
  // Power mode should be interpreted in the following way:
  //   LOW:
  //     - Connection interval = ~11.25ms - 15ms
  //   HIGH:
  //     - Connection interval = ~100ms - 125ms
  virtual Ptr<ClientGATTConnection> connectToGATTServer(
      Ptr<BLEPeripheralV2> peripheral,
      MTU mtu,
      PowerMode::Value power_mode,
      Ptr<ClientGATTConnectionLifecycleCallback>
          connection_lifecycle_callback) = 0;

  // Establishes a BLE socket to the specified remote peripheral. Returns a null
  // Ptr on error.
  virtual Ptr<BLESocketV0> establishBLESocket(
      Ptr<BLEPeripheralV2> ble_peripheral,
      Ptr<BLESocketLifecycleCallback> socket_lifecycle_callback) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_BLE_V2_H_
