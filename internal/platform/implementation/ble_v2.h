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
#include <memory>
#include <set>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/output_stream.h"

namespace location {
namespace nearby {
namespace api {
namespace ble_v2 {

// Coarse representation of power settings throughout all BLE operations.
enum class PowerMode {
  kUnknown = 0,
  kUltraLow = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
};

// https://developer.android.com/reference/android/bluetooth/le/AdvertiseData
//
// Bundle of data found in a BLE advertisement.
//
// All service UUIDs will conform to the 16-bit Bluetooth base UUID,
// 0000xxxx-0000-1000-8000-00805F9B34FB. This makes it possible to store two
// byte service UUIDs in the advertisement.
struct BleAdvertisementData {
  using TxPowerLevel = std::int8_t;

  static constexpr TxPowerLevel kUnspecifiedTxPowerLevel =
      std::numeric_limits<TxPowerLevel>::min();

  bool is_connectable;

  // If tx_power_level is not set to kUnspecifiedTxPowerLevel, platform
  // implementer needs to set the TxPowerLevel.
  TxPowerLevel tx_power_level;

  // If the set is not empty, the platform implementer needs to add the
  // service_uuids in the advertisement data.
  absl::flat_hash_set<std::string> service_uuids;

  // Maps service UUIDs to their service data.
  //
  // Note if platform can't advertise data from Data type (0x16)
  // (reaonly in iOS), then (iOS) should advertise data via LocalName data
  // type (0x08). It means the iOS should take the first index of service_data
  // as the data for LocalName type.
  absl::flat_hash_map<std::string, location::nearby::ByteArray> service_data;
};

// TODO(b/213835576): Refactor BlePeripheral. The one in BluetoothAdapter
// should be considered, too. Opaque wrapper over a BLE peripheral. Must be
// able to uniquely identify a peripheral so that we can connect to its GATT
// server.
class BlePeripheral {
 public:
  virtual ~BlePeripheral() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice#getAddress()
  //
  // This should be the MAC address when possible. If the implementation is
  // unable to retrieve that, any unique identifier should suffice.
  virtual std::string GetId() const = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic
//
// Representation of a GATT characteristic.
struct GattCharacteristic {
  enum class Permission {
    kUnknown = 0,
    kRead = 1,
    kWrite = 2,
    kLast,
  };

  enum class Property {
    kUnknown = 0,
    kRead = 1,
    kWrite = 2,
    kIndicate = 3,
    kLast,
  };

  std::string uuid;
  std::string servie_uuid;

  // Hashable
  template <typename H>
  friend H AbslHashValue(H h, const GattCharacteristic& s) {
    return H::combine(std::move(h), s.uuid, s.servie_uuid);
  }
  bool operator==(const GattCharacteristic& rhs) const {
    return this->uuid == rhs.uuid && this->servie_uuid == rhs.servie_uuid;
  }
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGatt
//
// Representation of a client GATT connection to a remote GATT server.
class ClientGattConnection {
 public:
  virtual ~ClientGattConnection() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#getDevice()
  virtual BlePeripheral& GetPeripheral() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#discoverServices()
  //
  // Discovers all available services and characteristics on this connection.
  // Returns whether or not discovery finished successfully.
  //
  // This function should block until discovery has finished.
  virtual bool DiscoverServices() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#getService(java.util.UUID)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattService.html#getCharacteristic(java.util.UUID)
  //
  // Retrieves a GATT characteristic. On error, does not return a value.
  //
  // DiscoverServices() should be called before this method to fetch all
  // available services and characteristics first.
  //
  // It is okay for duplicate services to exist, as long as the specified
  // characteristic UUID is unique among all services of the same UUID.
  virtual absl::optional<GattCharacteristic> GetCharacteristic(
      absl::string_view service_uuid,
      absl::string_view characteristic_uuid) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#readCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#getValue()
  virtual absl::optional<ByteArray> ReadCharacteristic(
      const GattCharacteristic& characteristic) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#writeCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  //
  // Sends a remote characteristic write request to the server and returns
  // whether or not it was successful.
  virtual bool WriteCharacteristic(const GattCharacteristic& characteristic,
                                   const ByteArray& value) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#disconnect()
  virtual void Disconnect() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattServer
//
// Representation of a server GATT connection to a remote GATT client.
class ServerGattConnection {
 public:
  virtual ~ServerGattConnection() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattServer.html#notifyCharacteristicChanged(android.bluetooth.BluetoothDevice,%20android.bluetooth.BluetoothGattCharacteristic,%20boolean)
  //
  // Sends a notification (via indication) to the client that a characteristic
  // has changed with the given value. Returns whether or not it was
  // successful.
  //
  // The value sent does not have to reflect the locally stored characteristic
  // value. To update the local value, call GattServer::UpdateCharacteristic.
  virtual bool SendCharacteristic(const GattCharacteristic& characteristic,
                                  const ByteArray& value) = 0;
};

// Callback for asynchronous events on the client side of a GATT connection.
struct ClientGattConnectionCallback {
 public:
  // Called when the client is disconnected from the GATT server.
  std::function<void(ClientGattConnection& connection)> disconnected_cb =
      DefaultCallback<ClientGattConnection&>();
};

// Callback for asynchronous events on the server side of a GATT connection.
struct ServerGattConnectionCallback {
  // Called when a remote peripheral connected to us and subscribed to one of
  // our characteristics.
  std::function<void(ServerGattConnection& connection,
                     const GattCharacteristic& characteristic)>
      characteristic_subscription_cb;

  // Called when a remote peripheral unsubscribed from one of our
  // characteristics.
  std::function<void(ServerGattConnection& connection,
                     const GattCharacteristic& characteristic)>
      characteristic_unsubscription_cb;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattServer
//
// Representation of a BLE GATT server.
class GattServer {
 public:
  virtual ~GattServer() = default;

  // Creates a characteristic and adds it to the GATT server under the given
  // characteristic and service UUIDs. Returns no value upon error.
  //
  // Characteristics of the same service UUID should be put under one
  // service rather than many services with the same UUID.
  //
  // If the INDICATE property is included, the characteristic should include
  // the official Bluetooth Client Characteristic Configuration descriptor
  // with UUID 0x2902 and a WRITE permission. This allows remote clients to
  // write to this descriptor and subscribe for characteristic changes. For
  // more information about this descriptor, please go to:
  // https://www.bluetooth.com/specifications/Gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.Gatt.client_characteristic_configuration.xml
  virtual absl::optional<GattCharacteristic> CreateCharacteristic(
      absl::string_view service_uuid, absl::string_view characteristic_uuid,
      const std::vector<GattCharacteristic::Permission>& permissions,
      const std::vector<GattCharacteristic::Property>& properties) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  //
  // Locally updates the value of a characteristic and returns whether or not
  // it was successful.
  virtual bool UpdateCharacteristic(
      const GattCharacteristic& characteristic,
      const location::nearby::ByteArray& value) = 0;

  // Stops a GATT server.
  virtual void Stop() = 0;
};

class BleSocket {
 public:
  virtual ~BleSocket() {}

  // Returns the remote BLE peripheral tied to this socket.
  virtual BlePeripheral& GetRemotePeripheral() = 0;

  // Writes a message on the socket and blocks until finished. Returns
  // Exception::kIo upon error, and Exception::kSuccess otherwise.
  virtual Exception Write(const ByteArray& message) = 0;

  // Closes the socket and blocks until finished. Returns Exception::kIo upon
  // error, and Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
  virtual InputStream& GetInputStream() = 0;
  virtual OutputStream& GetOutputStream() = 0;
};

// Callback for asynchronous events on a BleSocket object.
class BleSocketLifeCycleCallback {
 public:
  virtual ~BleSocketLifeCycleCallback() = default;

  // Called when a message arrives on a socket.
  virtual void OnMessageReceived(BleSocket* socket,
                                 const ByteArray& message) = 0;

  // Called when a socket gets disconnected.
  virtual void OnDisconnected(BleSocket* socket) = 0;
};

// Callback for asynchronous events on the server side of a BleSocket object.
class ServerBleSocketLifeCycleCallback : public BleSocketLifeCycleCallback {
 public:
  ~ServerBleSocketLifeCycleCallback() override {}

  // Called when a new incoming socket has been established.
  virtual void OnSocketEstablished(BleSocket* socket) = 0;
};

// The main BLE medium used inside of Nearby. This serves as the entry point
// for all BLE and GATT related operations.
class BleMedium {
 public:
  using Mtu = uint32_t;

  virtual ~BleMedium() = default;

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeAdvertiser.html#startAdvertising(android.bluetooth.le.AdvertiseSettings,%20android.bluetooth.le.AdvertiseData,%20android.bluetooth.le.AdvertiseData,%20android.bluetooth.le.AdvertiseCallback)
  //
  // Starts BLE advertising and returns whether or not it was successful.
  //
  // Power mode should be interpreted in the following way:
  //   LOW:
  //     - TX power = medium
  //   HIGH:
  //     - TX power = high
  virtual bool StartAdvertising(const BleAdvertisementData& advertising_data,
                                const BleAdvertisementData& scan_response_data,
                                PowerMode power_mode) = 0;

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeAdvertiser.html#stopAdvertising(android.bluetooth.le.AdvertiseCallback)
  //
  // Stops advertising.
  virtual bool StopAdvertising() = 0;

  // https://developer.android.com/reference/android/bluetooth/le/ScanCallback
  //
  // Callback for BLE scan results.
  //
  // The passed in advertisement_data is the merged combination of both
  // advertisement data and scan response.
  //
  // Every discovery of an advertisement should be reported, even if the
  // advertisement was discovered before.
  //
  // The peripheral is owned by platform implementation and it should outlive
  // for the whole peripheral(device) connection life cycle.
  struct ScanCallback {
    std::function<void(BlePeripheral& peripheral,
                       const BleAdvertisementData& advertisement_data)>
        advertisement_found_cb =
            DefaultCallback<BlePeripheral&, const BleAdvertisementData&>();
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
  virtual bool StartScanning(const std::vector<std::string>& service_uuids,
                             PowerMode power_mode, ScanCallback callback) = 0;

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeScanner.html#stopScan(android.bluetooth.le.ScanCallback)
  //
  // Stops scanning.
  virtual bool StopScanning() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothManager#openGattServer(android.content.Context,%20android.bluetooth.BluetoothGattServerCallback)
  //
  // Starts a GATT server. Returns a nullptr upon error.
  virtual std::unique_ptr<GattServer> StartGattServer(
      ServerGattConnectionCallback callback) = 0;

  // Starts listening for incoming BLE sockets and returns false upon error.
  virtual bool StartListeningForIncomingBleSockets(
      const ServerBleSocketLifeCycleCallback& callback) = 0;

  // Stops listening for incoming BLE sockets.
  virtual void StopListeningForIncomingBleSockets() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#connectGatt(android.content.Context,%20boolean,%20android.bluetooth.BluetoothGattCallback)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#requestConnectionPriority(int)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#requestMtu(int)
  //
  // Connects to a GATT server and negotiates the specified connection
  // parameters. Returns nullptr upon error.
  //
  // Both connection interval and MTU can be negotiated on a best-effort
  // basis.
  //
  // Power mode should be interpreted in the following way:
  //   HIGH:
  //     - Connection interval = ~11.25ms - 15ms
  //   LOW:
  //     - Connection interval = ~100ms - 125ms
  virtual std::unique_ptr<ClientGattConnection> ConnectToGattServer(
      BlePeripheral& peripheral, Mtu mtu, PowerMode power_mode,
      ClientGattConnectionCallback callback) = 0;

  // Establishes a BLE socket to the specified remote peripheral. Returns
  // nullptr on error.
  virtual std::unique_ptr<BleSocket> EstablishBleSocket(
      BlePeripheral* peripheral,
      const BleSocketLifeCycleCallback& callback) = 0;
};

}  // namespace ble_v2
}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_BLE_V2_H_
