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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_H_
#define CORE_INTERNAL_MEDIUMS_BLE_H_

#include <cstdint>

#include "core/internal/mediums/bluetooth_radio.h"
#include "platform/api/ble.h"
#include "platform/api/bluetooth_adapter.h"
#include "platform/api/lock.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
class BLE {
 public:
  explicit BLE(Ptr<BluetoothRadio<Platform>> bluetooth_radio);
  ~BLE();

  bool isAvailable();

  bool startAdvertising(const string& service_id,
                        ConstPtr<ByteArray> advertisement);
  void stopAdvertising();

  class DiscoveredPeripheralCallback {
   public:
    virtual ~DiscoveredPeripheralCallback() {}

    virtual void onPeripheralDiscovered(Ptr<BLEPeripheral> ble_peripheral,
                                        const string& service_id,
                                        ConstPtr<ByteArray> advertisement) = 0;
    virtual void onPeripheralLost(Ptr<BLEPeripheral> ble_peripheral,
                                  const string& service_id) = 0;
  };

  bool startScanning(
      const string& service_id,
      Ptr<DiscoveredPeripheralCallback> discovered_peripheral_callback);
  void stopScanning();

  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() {}

    virtual void onConnectionAccepted(Ptr<BLESocket> socket,
                                      const string& service_id) = 0;
  };

  bool startAcceptingConnections(
      const string& service_id,
      Ptr<AcceptedConnectionCallback> accepted_connection_callback);
  void stopAcceptingConnections();
  bool isAcceptingConnections();

  Ptr<BLESocket> connect(Ptr<BLEPeripheral> ble_peripheral,
                         const string& service_id);

 private:
  // TODO(ahlee): Rename to DiscoveredPeripheralCallbackBridge
  class BLEDiscoveredPeripheralCallback
      : public BLEMedium::DiscoveredPeripheralCallback {
   public:
    explicit BLEDiscoveredPeripheralCallback(
        Ptr<BLE::DiscoveredPeripheralCallback> discovered_peripheral_callback)
        : discovered_peripheral_callback_(discovered_peripheral_callback) {}
    ~BLEDiscoveredPeripheralCallback() override {
      // Nothing to do.
    }

    void onPeripheralDiscovered(Ptr<BLEPeripheral> ble_peripheral,
                                const string& service_id,
                                ConstPtr<ByteArray> advertisement) override {
      discovered_peripheral_callback_->onPeripheralDiscovered(
          ble_peripheral, service_id, advertisement);
    }
    void onPeripheralLost(Ptr<BLEPeripheral> ble_peripheral,
                          const string& service_id) override {
      discovered_peripheral_callback_->onPeripheralLost(ble_peripheral,
                                                        service_id);
    }

   private:
    ScopedPtr<Ptr<BLE::DiscoveredPeripheralCallback>>
        discovered_peripheral_callback_;
  };

  // TODO(ahlee): Rename to AcceptedConnectionCallbackBridge
  class BLEAcceptedConnectionCallback
      : public BLEMedium::AcceptedConnectionCallback {
   public:
    explicit BLEAcceptedConnectionCallback(
        Ptr<BLE::AcceptedConnectionCallback> accepted_connection_callback)
        : accepted_connection_callback_(accepted_connection_callback) {}
    ~BLEAcceptedConnectionCallback() override {
      // Nothing to do.
    }

    void onConnectionAccepted(Ptr<BLESocket> ble_socket,
                              const string& service_id) override {
      accepted_connection_callback_->onConnectionAccepted(ble_socket,
                                                          service_id);
    }

   private:
    ScopedPtr<Ptr<BLE::AcceptedConnectionCallback>>
        accepted_connection_callback_;
  };

  struct ScanningInfo {
    ScanningInfo(
        const string& service_id,
        Ptr<BLEDiscoveredPeripheralCallback> ble_discovered_peripheral_callback)
        : service_id(service_id),
          ble_discovered_peripheral_callback(
              ble_discovered_peripheral_callback) {}
    ~ScanningInfo() {
      // Nothing to do (the ScopedPtr members take care of themselves).
    }

    const string service_id;
    ScopedPtr<Ptr<BLEDiscoveredPeripheralCallback>>
        ble_discovered_peripheral_callback;
  };

  struct AdvertisingInfo {
    explicit AdvertisingInfo(const string& service_id)
        : service_id(service_id) {}
    ~AdvertisingInfo() {}

    const string service_id;
  };

  struct AcceptingConnectionsInfo {
    AcceptingConnectionsInfo(
        const string& service_id,
        Ptr<BLEAcceptedConnectionCallback> ble_accepted_connection_callback)
        : service_id(service_id),
          ble_accepted_connection_callback(ble_accepted_connection_callback) {}
    ~AcceptingConnectionsInfo() {
      // Nothing to do (the ScopedPtr members take care of themselves).
    }

    const string service_id;
    ScopedPtr<Ptr<BLEAcceptedConnectionCallback>>
        ble_accepted_connection_callback;
  };

  static const std::int32_t kMaxAdvertisementLength;

  bool isAdvertising();
  bool isScanning();

  // ------------ GENERAL ------------

  ScopedPtr<Ptr<Lock>> lock_;

  // ------------ CORE BLE ------------

  Ptr<BluetoothRadio<Platform>> bluetooth_radio_;
  ScopedPtr<Ptr<BluetoothAdapter>> bluetooth_adapter_;
  // The underlying, per-platform implementation.
  ScopedPtr<Ptr<BLEMedium>> ble_medium_;

  // ------------ DISCOVERY ------------

  // A bundle of state required to start/stop BLE scanning. When non-null,
  // we are currently performing a BLE scan.
  // In the Java code this maps to the bleListener and
  // bleScanningMediumOperation.
  Ptr<ScanningInfo> scanning_info_;

  // ------------ ADVERTISING ------------

  // A bundle of state required to start/stop BLE advertising. When non-null,
  // we are currently advertising over BLE.
  // In the Java code this maps to bleAdvertiser, advertiseCallback, and
  // bleAdvertisingMediumOperation.
  Ptr<AdvertisingInfo> advertising_info_;

  // A bundle of state required to start/stop accepting BLE connections. When
  // non-null, we are currently accepting BLE connections.
  // In the Java code this maps to the bleServerSocket.
  Ptr<AcceptingConnectionsInfo> accepting_connections_info_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/ble.cc"

#endif  // CORE_INTERNAL_MEDIUMS_BLE_H_
