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
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
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
  using UniqueId = std::uint64_t;
  virtual ~BlePeripheral() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice#getAddress()
  //
  // Returns the current address.
  //
  // This will always be an empty string on Apple platforms.
  virtual std::string GetAddress() const = 0;

  // Returns an immutable unique identifier. The identifier must not change when
  // the BLE address is rotated.
  virtual UniqueId GetUniqueId() const = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic
//
// Representation of a GATT characteristic.
struct GattCharacteristic {
  // Represents the GATT characteristic permissions
  // This enumeration supports a bitwise combination of its member values.
  // |, &, |= of the values are legal.
  enum class Permission {
    kNone = 0,
    kRead = 1 << 0,
    kWrite = 1 << 1,
    kLast,
  };

  // Represents the GATT characteristic properties
  // This enumeration supports a bitwise combination of its member values.
  // |, &, |= of the values are legal.
  enum class Property {
    kNone = 0,
    kRead = 1 << 0,
    kWrite = 1 << 1,
    kIndicate = 1 << 2,
    kNotify = 1 << 3,
    kLast,
  };

  Uuid uuid;
  Uuid service_uuid;
  Permission permission;
  Property property;

  // overloading operator for enum class Permission and Property
  friend inline Permission operator|(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<int>(a) | static_cast<int>(b));
  }

  friend inline Permission operator&(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<int>(a) & static_cast<int>(b));
  }
  friend inline Permission& operator|=(Permission& a, Permission b) {
    a = a | b;
    return a;
  }

  friend inline Property operator|(Property a, Property b) {
    return static_cast<Property>(static_cast<int>(a) | static_cast<int>(b));
  }

  friend inline Property operator&(Property a, Property b) {
    return static_cast<Property>(static_cast<int>(a) & static_cast<int>(b));
  }
  friend inline Property& operator|=(Property& a, Property b) {
    a = a | b;
    return a;
  }

  // Hashable
  template <typename H>
  friend H AbslHashValue(H h, const GattCharacteristic& s) {
    return H::combine(std::move(h), s.uuid, s.service_uuid, s.permission,
                      s.property);
  }
  bool operator==(const GattCharacteristic& rhs) const {
    return this->uuid == rhs.uuid && this->service_uuid == rhs.service_uuid &&
           this->permission == rhs.permission && this->property == rhs.property;
  }
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const GattCharacteristic& c) {
    absl::Format(&sink, "(service:%s, uuid:%s, permission:%v, property%v)",
                 std::string(c.service_uuid), std::string(c.uuid), c.permission,
                 c.property);
  }
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGatt
//
// Representation of a client GATT connection to a remote GATT server.
class GattClient {
 public:
  // Specifies what type of GATT write should be performed.
  enum class WriteType {
    kWithResponse = 0,
    kWithoutResponse = 1,
  };

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
  virtual absl::optional<std::string> ReadCharacteristic(
      const GattCharacteristic& characteristic) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#writeCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  //
  // Sends a remote characteristic write request to the server and returns
  // whether or not it was successful.
  virtual bool WriteCharacteristic(const GattCharacteristic& characteristic,
                                   absl::string_view value, WriteType type) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#setCharacteristicNotification(android.bluetooth.BluetoothGattCharacteristic,%20boolean)
  //
  // Enable or disable notifications/indications for a given characteristic.
  virtual bool SetCharacteristicSubscription(
      const GattCharacteristic& characteristic, bool enable,
      absl::AnyInvocable<void(absl::string_view value)>
          on_characteristic_changed_cb) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#disconnect()
  virtual void Disconnect() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGattServer
//
// Representation of a BLE GATT server.
// LINT.IfChange
class GattServer {
 public:
  virtual ~GattServer() = default;

  // Returns the local BlePeripheral.
  virtual BlePeripheral& GetBlePeripheral() = 0;

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
      GattCharacteristic::Permission permission,
      GattCharacteristic::Property property) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  //
  // Locally updates the value of a characteristic and returns whether or not
  // it was successful.
  virtual bool UpdateCharacteristic(const GattCharacteristic& characteristic,
                                    const nearby::ByteArray& value) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattServer#notifyCharacteristicChanged(android.bluetooth.BluetoothDevice,%20android.bluetooth.BluetoothGattCharacteristic,%20boolean,%20byte[])
  //
  // Send a notification or indication that a local characteristic has been
  // updated and returns an absl::Status indicating success or what went wrong.
  virtual absl::Status NotifyCharacteristicChanged(
      const GattCharacteristic& characteristic, bool confirm,
      const ByteArray& new_value) = 0;

  // Stops a GATT server.
  virtual void Stop() = 0;
};
// LINT.ThenChange(//depot/google3/third_party/nearby/internal/platform/ble_v2.h)

// Callback for asynchronous events on the client side of a GATT connection.
struct ClientGattConnectionCallback {
 public:
  // Called when the characteristic is changed
  absl::AnyInvocable<void(absl::string_view value)>
      on_characteristic_changed_cb = [](absl::string_view) {};

  // Called when the client is disconnected from the GATT server.
  absl::AnyInvocable<void()> disconnected_cb = []() {};
};

// Callback for asynchronous events on the server side of a GATT connection.
struct ServerGattConnectionCallback {
  using ReadValueCallback =
      absl::AnyInvocable<void(absl::StatusOr<absl::string_view> data)>;
  using WriteValueCallback = absl::AnyInvocable<void(absl::Status result)>;
  // Called when a remote peripheral connected to us and subscribed to one of
  // our characteristics.
  absl::AnyInvocable<void(const GattCharacteristic& characteristic)>
      characteristic_subscription_cb;

  // Called when a remote peripheral unsubscribed from one of our
  // characteristics.
  absl::AnyInvocable<void(const GattCharacteristic& characteristic)>
      characteristic_unsubscription_cb;

  // Called when a gatt client is reading from the characteristic.
  // Must call `callback` with the read result.
  // When a characteristic has a static value set with
  // `GattServer::UpdateCharacteristic()`, then reading from the characteristic
  // yields that static value. The read callback is not called.
  // Otherwise, the gatt server calls the read callback to get the value.
  absl::AnyInvocable<void(const BlePeripheral& remote_device,
                          const GattCharacteristic& characteristic, int offset,
                          ReadValueCallback callback)>
      on_characteristic_read_cb;

  // Called when a gatt client is writing to the characteristic.
  // Must call `callback` with the write result.
  absl::AnyInvocable<void(const BlePeripheral& remote_device,
                          const GattCharacteristic& characteristic, int offset,
                          absl::string_view data, WriteValueCallback callback)>
      on_characteristic_write_cb;
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
  using GetRemotePeripheralCallback = absl::AnyInvocable<void(BlePeripheral&)>;
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
    absl::AnyInvocable<void(BlePeripheral& peripheral)>
        advertisement_lost_cb = [](BlePeripheral&) {};
  };

  // Async interface for StartScanning.
  // Result status will be passed to start_scanning_result callback.
  // To stop scanning, invoke the stop_scanning callback in
  // ScanningSession.
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

  // Calls `callback` and returns true if `mac_address` is a valid BLE address.
  // Otherwise, does not call the callback and returns false.
  //
  // This method is not available on Apple platforms and will always return
  // false, ignoring the callback.
  virtual bool GetRemotePeripheral(const std::string& mac_address,
                                   GetRemotePeripheralCallback callback) = 0;

  // Calls `callback` and returns true if `id` refers to a known BLE peripheral.
  // Otherwise, does not call the callback and returns false.
  virtual bool GetRemotePeripheral(BlePeripheral::UniqueId id,
                                   GetRemotePeripheralCallback callback) = 0;
};

}  // namespace ble_v2
}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_BLE_V2_H_
