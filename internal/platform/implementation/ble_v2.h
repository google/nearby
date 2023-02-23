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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace api {
namespace ble_v2 {

// Coarse representation of power settings throughout all BLE operations.
enum class TxPowerLevel {
  kUnknown = 0,
  kUltraLow = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
};

// https://developer.android.com/reference/android/bluetooth/le/AdvertisingSetParameters.Builder
//
// The preferences for Advertising.
struct AdvertiseParameters {
  // The transmission power level for the advertising.
  TxPowerLevel tx_power_level;
  // Whether the advertisement type should be connectable or non-connectable.
  bool is_connectable;
};

// https://developer.android.com/reference/android/bluetooth/le/AdvertiseData
//
// Bundle of data found in a BLE advertisement.
//
// All service UUIDs will conform to the 16-bit Bluetooth base UUID,
// 0000xxxx-0000-1000-8000-00805F9B34FB. This makes it possible to store two
// byte service UUIDs in the advertisement.
struct BleAdvertisementData {
  // Broadcasts a BLE extended advertisement if it is true.
  bool is_extended_advertisement;

  // Maps service UUIDs to their service data.
  //
  // For each platform should follow to set the service UUID(key) and service
  // data(value):
  //
  // iOS    : 16 bit service UUID (type=0x03) + LocalName data (type=0x08)
  // Windows: Service data (type=0x16)
  // Android: 16 bit service UUID (type=0x03) + Service data (type=0x16)
  absl::flat_hash_map<Uuid, nearby::ByteArray> service_data;
};

// Opaque wrapper over a BLE peripheral. Must be able to uniquely identify a
// peripheral so that we can connect to its GATT server.
class BlePeripheral {
 public:
  virtual ~BlePeripheral() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice#getAddress()
  //
  // This should be the MAC address when possible. If the implementation is
  // unable to retrieve that, any unique identifier should suffice.
  virtual std::string GetAddress() const = 0;
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

  Uuid uuid;
  Uuid service_uuid;
  std::vector<Permission> permissions;
  std::vector<Property> properties;

  // Hashable
  template <typename H>
  friend H AbslHashValue(H h, const GattCharacteristic& s) {
    return H::combine(std::move(h), s.uuid, s.service_uuid, s.permissions,
                      s.properties);
  }
  bool operator==(const GattCharacteristic& rhs) const {
    bool has_equal_permissions =
        std::is_permutation(this->permissions.begin(), this->permissions.end(),
                            rhs.permissions.begin(), rhs.permissions.end());
    bool has_equal_properties =
        std::is_permutation(this->properties.begin(), this->properties.end(),
                            rhs.properties.begin(), rhs.properties.end());
    return this->uuid == rhs.uuid && this->service_uuid == rhs.service_uuid &&
           has_equal_permissions && has_equal_properties;
  }
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGatt
//
// Representation of a client GATT connection to a remote GATT server.
class GattClient {
 public:
  virtual ~GattClient() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#discoverServices()
  //
  // Discovers available service and characteristics on this connection.
  // Returns whether or not discovery finished successfully.
  //
  // This function should block until discovery has finished.
  virtual bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#getService(java.util.UUID)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattService.html#getCharacteristic(java.util.UUID)
  //
  // Retrieves a GATT characteristic. On error, does not return a value.
  //
  // DiscoverServiceAndCharacteristics() should be called before this method to
  // fetch all available services and characteristics first.
  //
  // It is okay for duplicate services to exist, as long as the specified
  // characteristic UUID is unique among all services of the same UUID.
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  virtual absl::optional<GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#readCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#getValue()
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
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
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  virtual absl::optional<GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
      const std::vector<GattCharacteristic::Permission>& permissions,
      const std::vector<GattCharacteristic::Property>& properties) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  //
  // Locally updates the value of a characteristic and returns whether or not
  // it was successful.
  virtual bool UpdateCharacteristic(const GattCharacteristic& characteristic,
                                    const nearby::ByteArray& value) = 0;

  // Stops a GATT server.
  virtual void Stop() = 0;
};

// Callback for asynchronous events on the client side of a GATT connection.
struct ClientGattConnectionCallback {
 public:
  // Called when the client is disconnected from the GATT server.
  absl::AnyInvocable<void()> disconnected_cb = []() {};
};

// Callback for asynchronous events on the server side of a GATT connection.
struct ServerGattConnectionCallback {
  // Called when a remote peripheral connected to us and subscribed to one of
  // our characteristics.
  absl::AnyInvocable<void(const GattCharacteristic& characteristic)>
      characteristic_subscription_cb;

  // Called when a remote peripheral unsubscribed from one of our
  // characteristics.
  absl::AnyInvocable<void(const GattCharacteristic& characteristic)>
      characteristic_unsubscription_cb;
};

// A BLE GATT client socket for requesting GATT socket.
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

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  virtual BlePeripheral* GetRemotePeripheral() = 0;
};

// A BLE GATT server socket for listening incoming GATT socket.
class BleServerSocket {
 public:
  virtual ~BleServerSocket() = default;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  virtual std::unique_ptr<BleSocket> Accept() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// The main BLE medium used inside of Nearby. This serves as the entry point
// for all BLE and GATT related operations.
class BleMedium {
 public:
  virtual ~BleMedium() = default;

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeAdvertiser.html#startAdvertising(android.bluetooth.le.AdvertiseSettings,%20android.bluetooth.le.AdvertiseData,%20android.bluetooth.le.AdvertiseData,%20android.bluetooth.le.AdvertiseCallback)
  //
  // Starts BLE advertising and returns whether or not it was successful.
  virtual bool StartAdvertising(
      const BleAdvertisementData& advertising_data,
      AdvertiseParameters advertise_set_parameters) = 0;

  struct AdvertisingCallback {
    absl::AnyInvocable<void(absl::Status)> start_advertising_result;
  };

  struct AdvertisingSession {
    absl::AnyInvocable<absl::Status()> stop_advertising;
  };

  // Async interface for StartAdertising.
  // Result status will be passed to start_advertising_result callback.
  // To stop advertising, invoke the stop_advertising callback in
  // AdvertisingSession.
  virtual std::unique_ptr<AdvertisingSession> StartAdvertising(
      const BleAdvertisementData& advertising_data,
      AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) = 0;

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
    absl::AnyInvocable<void(BlePeripheral& peripheral,
                            BleAdvertisementData advertisement_data)>
        advertisement_found_cb = [](BlePeripheral&, BleAdvertisementData) {};
  };

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeScanner.html#startScan(java.util.List%3Candroid.bluetooth.le.ScanFilter%3E,%20android.bluetooth.le.ScanSettings,%20android.bluetooth.le.ScanCallback)
  //
  // Starts scanning and returns whether or not it was successful.
  //
  // TX Power level should be interpreted in the following way:
  //   LOW:
  //     - Scan window = ~512ms
  //     - Scan interval = ~5120ms
  //   HIGH:
  //     - Scan window = ~4096ms
  //     - Scan interval = ~4096ms
  virtual bool StartScanning(const Uuid& service_uuid,
                             TxPowerLevel tx_power_level,
                             ScanCallback callback) = 0;

  // https://developer.android.com/reference/android/bluetooth/le/BluetoothLeScanner.html#stopScan(android.bluetooth.le.ScanCallback)
  //
  // Stops scanning.
  virtual bool StopScanning() = 0;

  struct ScanningSession {
    absl::AnyInvocable<absl::Status()> stop_scanning;
  };

  struct ScanningCallback {
    absl::AnyInvocable<void(absl::Status)> start_scanning_result =
        [](absl::Status) {};
    absl::AnyInvocable<void(BlePeripheral& peripheral,
                            BleAdvertisementData advertisement_data)>
        advertisement_found_cb = [](BlePeripheral&, BleAdvertisementData) {};
  };

  // Async interface for StartScanning.
  // Result status will be passed to start_advertising_result callback.
  // To stop advertising, invoke the stop_advertising callback in
  // AdvertisingSession.
  virtual std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid, TxPowerLevel tx_power_level,
      ScanningCallback callback) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothManager#openGattServer(android.content.Context,%20android.bluetooth.BluetoothGattServerCallback)
  //
  // Starts a GATT server. Returns a nullptr upon error.
  virtual std::unique_ptr<GattServer> StartGattServer(
      ServerGattConnectionCallback callback) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#connectGatt(android.content.Context,%20boolean,%20android.bluetooth.BluetoothGattCallback)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#requestConnectionPriority(int)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#requestMtu(int)
  //
  // Connects to a GATT server and negotiates the specified connection
  // parameters. Returns nullptr upon error.
  //
  // TX Power level should be interpreted in the following way:
  //   HIGH:
  //     - Connection interval = ~11.25ms - 15ms
  //   LOW:
  //     - Connection interval = ~100ms - 125ms
  virtual std::unique_ptr<GattClient> ConnectToGattServer(
      BlePeripheral& peripheral, TxPowerLevel tx_power_level,
      ClientGattConnectionCallback callback) = 0;

  // Opens a BLE server socket based on service ID.
  //
  // On success, returns a new BleServerSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<BleServerSocket> OpenServerSocket(
      const std::string& service_id) = 0;

  // Connects to a BLE peripheral.
  //
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<BleSocket> Connect(
      const std::string& service_id, TxPowerLevel tx_power_level,
      BlePeripheral& peripheral, CancellationFlag* cancellation_flag) = 0;

  // Requests if support extended advertisement.
  virtual bool IsExtendedAdvertisementsAvailable() = 0;
};

}  // namespace ble_v2
}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_BLE_V2_H_
