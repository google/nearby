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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_H_

#include <cstdint>

#include "core/internal/mediums/advertisement_read_result.h"
#include "core/internal/mediums/ble_advertisement.h"
#include "core/internal/mediums/bluetooth_radio.h"
#include "core/internal/mediums/discovered_peripheral_callback.h"
#include "core/internal/mediums/discovered_peripheral_tracker.h"
#include "platform/api/ble_v2.h"
#include "platform/api/bluetooth_adapter.h"
#include "platform/api/hash_utils.h"
#include "platform/api/lock.h"
#include "platform/byte_array.h"
#include "platform/cancelable_alarm.h"
#include "platform/port/string.h"
#include "platform/prng.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace ble_v2 {

template <typename>
class ProcessOnLostRunnable;

template <typename>
class OnAdvertisementFoundRunnable;

}  // namespace ble_v2

template <typename Platform>
class BLEV2 {
 public:
  explicit BLEV2(Ptr<BluetoothRadio<Platform>> bluetooth_radio);
  ~BLEV2();

  bool isAvailable();
  // While the start* functions for each action (advertising, scanning,
  // accepting connections) take in a service_id, the stop* and is* functions do
  // not. This is because the service_id isn't used. In the java code, shutdown
  // calls all the stop* functions w/ a null service_id. The service_id is just
  // passed through to the corresponding is* function, which ignores it.
  // service_id should be added back in when C++ supports multi-client.
  bool startAdvertising(const string& service_id,
                        ConstPtr<ByteArray> advertisement,
                        BLEMediumV2::PowerMode::Value power_mode,
                        const string& fast_advertisement_service_uuid);
  void stopAdvertising();

  bool startScanning(
      const string& service_id,
      Ptr<DiscoveredPeripheralCallback> discovered_peripheral_callback,
      BLEMediumV2::PowerMode::Value power_mode,
      const string& fast_advertisement_service_uuid);
  void stopScanning();

  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() {}

    // TODO(ahlee): Add in connecting logic.
  };

  bool isAcceptingConnections();
  bool startAcceptingConnections(
      const string& service_id,
      Ptr<AcceptedConnectionCallback> accepted_connection_callback);
  void stopAcceptingConnections();

 private:
  template <typename>
  friend class ble_v2::ProcessOnLostRunnable;
  template <typename>
  friend class ble_v2::OnAdvertisementFoundRunnable;

  class GATTAdvertisementFetcherFacade
      : public DiscoveredPeripheralTracker<Platform>::GattAdvertisementFetcher {
   public:
    explicit GATTAdvertisementFetcherFacade(Ptr<BLEV2<Platform>> impl)
        : impl_(impl) {}
    ~GATTAdvertisementFetcherFacade() override {}

    Ptr<AdvertisementReadResult<Platform>> fetchGattAdvertisements(
        Ptr<BLEPeripheralV2> ble_peripheral, std::int32_t num_slots,
        Ptr<AdvertisementReadResult<Platform>> advertisement_read_result)
        override {
      return impl_->processFetchGattAdvertisementsRequest(
          ble_peripheral, num_slots, advertisement_read_result);
    }

   private:
    Ptr<BLEV2<Platform>> impl_;
  };

  class ScanCallbackFacade : public BLEMediumV2::ScanCallback {
   public:
    explicit ScanCallbackFacade(Ptr<BLEV2<Platform>> impl) : impl_(impl) {}
    ~ScanCallbackFacade() override {}

    void onAdvertisementFound(
        Ptr<BLEPeripheralV2> peripheral,
        ConstPtr<BLEAdvertisementData> advertisement_data) override {
      impl_->onAdvertisementFoundImpl(peripheral, advertisement_data);
    }

   private:
    Ptr<BLEV2<Platform>> impl_;
  };

  class ClientGATTConnectionLifecycleCallbackFacade
      : public ClientGATTConnectionLifecycleCallback {
   public:
    explicit ClientGATTConnectionLifecycleCallbackFacade(
        Ptr<BLEV2<Platform>> impl)
        : impl_(impl) {}
    ~ClientGATTConnectionLifecycleCallbackFacade() override {}

    void onDisconnected(Ptr<ClientGATTConnection> connection) override {
      // Avoid leaks.
      ScopedPtr<Ptr<ClientGATTConnection>> scoped_connection(connection);

      // Nothing else to do for now.
    }

   private:
    Ptr<BLEV2<Platform>> impl_;
  };

  class ServerGATTConnectionLifecycleCallbackFacade
      : public ServerGATTConnectionLifecycleCallback {
   public:
    explicit ServerGATTConnectionLifecycleCallbackFacade(
        Ptr<BLEV2<Platform>> impl)
        : impl_(impl) {}
    ~ServerGATTConnectionLifecycleCallbackFacade() override {}

    void onCharacteristicSubscription(
        Ptr<ServerGATTConnection> connection,
        Ptr<GATTCharacteristic> characteristic) override {
      // Avoid leaks. Do not scope the characteristic because it is ref counted
      // by the per-platform ble_v2 implementation.
      ScopedPtr<Ptr<ServerGATTConnection>> scoped_connection(connection);

      // Nothing else to do for now.
    }

    void onCharacteristicUnsubscription(
        Ptr<ServerGATTConnection> connection,
        Ptr<GATTCharacteristic> characteristic) override {
      // Avoid leaks. Do not scope the characteristic because it is ref counted
      // by the per-platform ble_v2 implementation.
      ScopedPtr<Ptr<ServerGATTConnection>> scoped_connection(connection);

      // Nothing else to do for now.
    }

   private:
    Ptr<BLEV2<Platform>> impl_;
  };

  struct ScanningInfo {
    ScanningInfo(const string& service_id,
                 Ptr<ScanCallbackFacade> scan_callback_facade,
                 Ptr<CancelableAlarm> on_lost_alarm)
        : service_id(service_id),
          scan_callback_facade(scan_callback_facade),
          on_lost_alarm(on_lost_alarm) {}
    ~ScanningInfo() {
      // Nothing to do (the ScopedPtr members take care of themselves).
    }

    const string service_id;
    ScopedPtr<Ptr<ScanCallbackFacade>> scan_callback_facade;
    // TODO(ahlee): Change to recurring cancelable alarm
    ScopedPtr<Ptr<CancelableAlarm>> on_lost_alarm;
  };

  struct AdvertisingInfo {
    explicit AdvertisingInfo(const string& service_id)
        : service_id(service_id) {}
    ~AdvertisingInfo() {}

    const string service_id;
  };

  struct GATTServerInfo {
    GATTServerInfo(Ptr<GATTServer> gatt_server,
                   Ptr<ServerGATTConnectionLifecycleCallbackFacade>
                       connection_lifecycle_callback)
        : gatt_server(gatt_server),
          connection_lifecycle_callback(connection_lifecycle_callback) {}
    ~GATTServerInfo() {
      // Nothing to do (the ScopedPtr members take care of themselves).
    }

    ScopedPtr<Ptr<GATTServer>> gatt_server;
    ScopedPtr<Ptr<ServerGATTConnectionLifecycleCallbackFacade>>
        connection_lifecycle_callback;
  };

  struct AcceptingConnectionsInfo {
    explicit AcceptingConnectionsInfo(const string& service_id)
        : service_id(service_id) {}
    ~AcceptingConnectionsInfo() {
      // Nothing to do (the ScopedPtr members take care of themselves).
    }

    const string service_id;
    // TODO(ahlee): Fill in.
  };

  static const std::int32_t kNumAdvertisementSlots;
  static const std::int32_t kMaxAdvertisementLength;
  static const std::int32_t kDummyServiceIdLength;
  static const char* kCopresenceServiceUuid;
  static const std::int64_t kOnLostTimeoutMillis;
  static const std::int64_t kGattAdvertisementOperationTimeoutMillis;
  static const std::int64_t kMinConnectionAttemptRecoveryDurationMillis;
  static const std::int32_t kMaxConnectionAttemptRecoveryFuzzDurationMillis;
  static const std::uint32_t kDefaultMtu;
  static const std::int64_t kAdvertisementUuidMsb;
  static const std::int64_t kAdvertisementUuidLsb;

  bool isAdvertising();
  ConstPtr<ByteArray> createAdvertisementHeader(
      const string& service_id, ConstPtr<ByteArray> advertisement_bytes,
      bool is_fast_advertisement);

  bool isScanning();
  void onAdvertisementFoundImpl(
      Ptr<BLEPeripheralV2> ble_peripheral,
      ConstPtr<BLEAdvertisementData> advertisement_data);
  void processOnLostTimeout();
  Ptr<CancelableAlarm> createOnLostAlarm();

  bool isAdvertisementGattServerRunning();
  bool startAdvertisementGattServer(const string& service_id,
                                    ConstPtr<ByteArray> advertisement);
  bool internalStartAdvertisementGattServer(
      ConstPtr<ByteArray> legacy_ble_advertisement_bytes,
      ConstPtr<ByteArray> ble_advertisement_bytes);
  bool generateAdvertisementCharacteristic(
    std::int32_t slot, ConstPtr<ByteArray> advertisement,
    Ptr<GATTServer> gatt_server);
  void stopAdvertisementGattServer();

  Ptr<AdvertisementReadResult<Platform>> processFetchGattAdvertisementsRequest(
      Ptr<BLEPeripheralV2> peripheral, std::int32_t num_slots,
      Ptr<AdvertisementReadResult<Platform>> advertisement_read_result);
  Ptr<AdvertisementReadResult<Platform>>
  internalReadFromAdvertisementGattServer(
      Ptr<BLEPeripheralV2> ble_peripheral, std::int32_t num_slots,
      Ptr<AdvertisementReadResult<Platform>> advertisement_read_result);

  void offloadFromPlatformThread(Ptr<Runnable> runnable);
  // TODO(ahlee): Move these out to utils (also used by
  // DiscoveredPeripheralTracker).
  ConstPtr<ByteArray> generateAdvertisementHash(
      ConstPtr<ByteArray> advertisement_bytes);
  ConstPtr<ByteArray> generateServiceIdHash(
      BLEAdvertisement::Version::Value version, const string& service_id);

  // This maps to a helper function found in bluetoothlowenergy/Utils.java. In
  // the C++ code we moved it because it's only used here.
  string generateAdvertisementUuid(std::int32_t slot);

  // ------------ GENERAL ------------

  ScopedPtr<Ptr<Lock>> lock_;
  // Where we throw potentially blocking work off of the platform thread.
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType>>
      platform_thread_offloader_;
  ScopedPtr<Ptr<Prng>> prng_;
  ScopedPtr<Ptr<HashUtils>> hash_utils_;

  // ------------ CORE BLE ------------

  Ptr<BluetoothRadio<Platform>> bluetooth_radio_;
  ScopedPtr<Ptr<BluetoothAdapter>> bluetooth_adapter_;
  // The underlying, per-platform implementation.
  ScopedPtr<Ptr<BLEMediumV2>> ble_medium_;

  // ------------ DISCOVERY ------------

  // scanning_info_ is not scoped because it's nullable.
  Ptr<ScanningInfo> scanning_info_;
  ScopedPtr<Ptr<DiscoveredPeripheralTracker<Platform>>>
      discovered_peripheral_tracker_;
  ScopedPtr<Ptr<typename Platform::ScheduledExecutorType>> on_lost_executor_;

  // ------------ ADVERTISING ------------

  // advertising_info_, gatt_server_info_, and accepting_connections_info_ are
  // not scoped because they are nullable.
  Ptr<AdvertisingInfo> advertising_info_;
  Ptr<GATTServerInfo> gatt_server_info_;
  Ptr<AcceptingConnectionsInfo> accepting_connections_info_;
  std::shared_ptr<BLEV2> self_{this, [](void*){}};
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/ble_v2.cc"

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_H_
