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

#ifndef CAL_API_BLE_H_
#define CAL_API_BLE_H_

#include <map>
#include <memory>
#include <set>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "third_party/nearby_connections/cpp/cal/base/ble_types.h"
namespace nearby {
namespace cal {
namespace api {

class BlePeripheral {
 public:
  virtual ~BlePeripheral() {}
  virtual std::string GetName() const = 0;
  virtual std::string GetAddress() const = 0;
  virtual ByteArray GetAdvertisementBytes() const = 0;
};

struct ScanResult {
  std::unique_ptr<BlePeripheral> peripheral;
  int rssi;
  int tx_power;
  absl::Time timestamp;
};

class GattCharacteristic {
 public:
  virtual ~GattCharacteristic() {}

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

  virtual std::string GetServiceUuid() = 0;
};

class ClientGattConnection {
 public:
  virtual ~ClientGattConnection() {}
  virtual BlePeripheral& GetPeripheral() = 0;
  virtual bool DiscoverServices() = 0;
  virtual absl::optional<GattCharacteristic*> GetCharacteristic(
      absl::string_view service_uuid,
      absl::string_view characteristic_uuid) = 0;
  virtual absl::optional<ByteArray> ReadCharacteristic(
      const GattCharacteristic& characteristic) = 0;
  virtual bool WriteCharacteristic(const GattCharacteristic& characteristic,
                                   const ByteArray& value) = 0;
  virtual void Disconnect() = 0;
  virtual bool SetCharacteristicNotification(
      const GattCharacteristic& characteristic, bool enable) = 0;
};

class ServerGattConnection {
 public:
  virtual ~ServerGattConnection() {}
  virtual bool SendCharacteristic(const GattCharacteristic& characteristic,
                                  const ByteArray& value) = 0;
};

class ClientGattConnectionLifeCycleCallback {
 public:
  virtual ~ClientGattConnectionLifeCycleCallback() {}
  virtual void OnDisconnected(ClientGattConnection* connection) = 0;
  virtual void onConnectionStateChange(ClientGattConnection* connection) = 0;
  virtual void onCharacteristicRead(ClientGattConnection* connection) = 0;
};

class ServerGattConnectionLifeCycleCallback {
 public:
  virtual ~ServerGattConnectionLifeCycleCallback() {}
  virtual void OnCharacteristicSubscription(
      ServerGattConnection* connection,
      const GattCharacteristic& characteristic) = 0;
  virtual void OnCharacteristicUnsubscription(
      ServerGattConnection* connection,
      const GattCharacteristic& characteristic) = 0;
};

class GattServer {
 public:
  virtual ~GattServer() {}
  virtual std::unique_ptr<GattCharacteristic> CreateCharacteristic(
      absl::string_view service_uuid, absl::string_view characteristic_uuid,
      const std::set<GattCharacteristic::Permission>& permissions,
      const std::set<GattCharacteristic::Property>& properties) = 0;
  virtual bool UpdateCharacteristic(const GattCharacteristic& characteristic,
                                    const ByteArray& value) = 0;
  virtual void Stop() = 0;
};

class BleSocket {
 public:
  virtual ~BleSocket() {}
  virtual api::BlePeripheral& GetRemotePeripheral() = 0;
  virtual Exception Write(const ByteArray& message) = 0;
  virtual Exception Close() = 0;
  virtual InputStream& GetInputStream() = 0;
  virtual OutputStream& GetOutputStream() = 0;
};

class BleSocketLifeCycleCallback {
 public:
  virtual ~BleSocketLifeCycleCallback() {}
  virtual void OnMessageReceived(BleSocket* socket,
                                 const ByteArray& message) = 0;
  virtual void OnDisconnected(BleSocket* socket) = 0;
};

class ServerBleSocketLifeCycleCallback : public BleSocketLifeCycleCallback {
 public:
  ~ServerBleSocketLifeCycleCallback() override {}
  virtual void OnSocketEstablished(BleSocket* socket) = 0;
};

class BleMedium {
 public:
  using Mtu = uint32_t;
  virtual ~BleMedium() {}
  virtual bool StartAdvertising(
      const BleAdvertisementData& advertisement_data) = 0;
  virtual void StopAdvertising(const std::string& service_id) = 0;
  class ScanCallback {
   public:
    virtual ~ScanCallback() {}
    virtual void OnAdvertisementFound(
        const ScanResult& scan_result,
        const BleAdvertisementData& advertisement_data) = 0;
  };
  virtual bool StartScanning(const std::set<std::string>& service_uuids,
                             PowerMode power_mode,
                             const ScanCallback& scan_callback) = 0;
  virtual void StopScanning() = 0;
  virtual std::unique_ptr<GattServer> StartGattServer(
      const ServerGattConnectionLifeCycleCallback& callback) = 0;
  virtual bool StartListeningForIncomingBleSockets(
      const ServerBleSocketLifeCycleCallback& callback) = 0;
  virtual void StopListeningForIncomingBleSockets() = 0;
  virtual std::unique_ptr<ClientGattConnection> ConnectToGattServer(
      BlePeripheral* peripheral, Mtu mtu, PowerMode power_mode,
      const ClientGattConnectionLifeCycleCallback& callback) = 0;
  virtual std::unique_ptr<BleSocket> EstablishBleSocket(
      BlePeripheral* peripheral,
      const BleSocketLifeCycleCallback& callback) = 0;
};

}  // namespace api
}  // namespace cal
}  // namespace nearby
#endif  // CAL_API_BLE_H_
