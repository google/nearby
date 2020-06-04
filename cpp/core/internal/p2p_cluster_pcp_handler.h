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

#ifndef CORE_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
#define CORE_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_

#include <vector>

#include "core/internal/bandwidth_upgrade_manager.h"
#include "core/internal/base_pcp_handler.h"
#include "core/internal/ble_advertisement.h"
#include "core/internal/ble_compat.h"
#include "core/internal/bluetooth_device_name.h"
#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/medium_manager.h"
#include "core/internal/pcp.h"
#include "core/internal/wifi_lan_service_info.h"
#include "core/options.h"
#include "core/strategy.h"
#include "platform/api/bluetooth_classic.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

// Concrete implementation of the PCPHandler for the P2P_CLUSTER PCP. This PCP
// is reserved for mediums that can connect to multiple devices simultaneously
// and all devices are considered equal. For asymmetric mediums, where one
// device is a server and the others are clients, use P2PStarPCPHandler instead.
//
// <p>Currently, this implementation advertises/discovers over BLE and Bluetooth
// and connects over Bluetooth.
template <typename Platform>
class P2PClusterPCPHandler : public BasePCPHandler<Platform> {
 public:
  P2PClusterPCPHandler(Ptr<MediumManager<Platform>> medium_manager,
                       Ptr<EndpointManager<Platform>> endpoint_manager,
                       Ptr<EndpointChannelManager> endpoint_channel_manager,
                       Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager);
  ~P2PClusterPCPHandler() override;

  Strategy getStrategy() override;
  PCP::Value getPCP() override;

 protected:
  std::vector<proto::connections::Medium> getConnectionMediumsByPriority()
      override;
  proto::connections::Medium getDefaultUpgradeMedium() override;

  // @PCPHandlerThread
  Ptr<typename BasePCPHandler<Platform>::StartOperationResult>
  startAdvertisingImpl(Ptr<ClientProxy<Platform>> client_proxy,
                       const string& service_id,
                       const string& local_endpoint_id,
                       const string& local_endpoint_name,
                       const AdvertisingOptions& options) override;

  // @PCPHandlerThread
  Status::Value stopAdvertisingImpl(
      Ptr<ClientProxy<Platform>> client_proxy) override;

  // @PCPHandlerThread
  Ptr<typename BasePCPHandler<Platform>::StartOperationResult>
  startDiscoveryImpl(Ptr<ClientProxy<Platform>> client_proxy,
                     const string& service_id,
                     const DiscoveryOptions& options) override;

  // @PCPHandlerThread
  Status::Value stopDiscoveryImpl(
      Ptr<ClientProxy<Platform>> client_proxy) override;

  // @PCPHandlerThread
  typename BasePCPHandler<Platform>::ConnectImplResult connectImpl(
      Ptr<ClientProxy<Platform>> client_proxy,
      Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint> endpoint)
      override;

 private:
  template <typename>
  friend class IncomingBluetoothConnectionProcessor;
  template <typename>
  friend class IncomingBleConnectionProcessor;
  template <typename>
  friend class IncomingWifiLanConnectionProcessor;
  template <typename>
  friend class FoundBluetoothAdvertisementProcessor;
  template <typename>
  friend class FoundBleAdvertisementProcessor;
  template <typename>
  friend class FoundWifiLanServiceProcessor;

  class IncomingBluetoothConnectionProcessor
      : public MediumManager<Platform>::IncomingBluetoothConnectionProcessor {
   public:
    IncomingBluetoothConnectionProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        const string& local_endpoint_name);

    void onIncomingBluetoothConnection(
        Ptr<BluetoothSocket> bluetooth_socket) override;

   private:
    class OnIncomingBluetoothConnectionRunnable : public Runnable {
     public:
      OnIncomingBluetoothConnectionRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<BluetoothSocket> bluetooth_socket);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<BluetoothSocket> bluetooth_socket_;
    };

    Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
    Ptr<ClientProxy<Platform>> client_proxy_;
    const string local_endpoint_name_;
  };

  class IncomingBleConnectionProcessor
      : public MediumManager<Platform>::IncomingBleConnectionProcessor {
   public:
    IncomingBleConnectionProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        const string& local_endpoint_name);

    void onIncomingBleConnection(Ptr<BLESocket> ble_socket,
                                 const string& service_id) override;

   private:
    class OnIncomingBleConnectionRunnable : public Runnable {
     public:
      OnIncomingBleConnectionRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy, Ptr<BLESocket> ble_socket);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<BLESocket> ble_socket_;
    };

    Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
    Ptr<ClientProxy<Platform>> client_proxy_;
    const string local_endpoint_name_;
  };

  class IncomingWifiLanConnectionProcessor
      : public MediumManager<Platform>::IncomingWifiLanConnectionProcessor {
   public:
    IncomingWifiLanConnectionProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        absl::string_view local_endpoint_name);

    void OnIncomingWifiLanConnection(
        Ptr<WifiLanSocket> wifi_lan_socket) override;

   private:
    class OnIncomingWifiLanConnectionRunnable : public Runnable {
     public:
      OnIncomingWifiLanConnectionRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<WifiLanSocket> wifi_lan_socket);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<WifiLanSocket> wifi_lan_socket_;
    };

    Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
    Ptr<ClientProxy<Platform>> client_proxy_;
    const string local_endpoint_name_;
  };

  class FoundBluetoothAdvertisementProcessor
      : public MediumManager<Platform>::FoundBluetoothDeviceProcessor {
   public:
    FoundBluetoothAdvertisementProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy, const string& service_id);

    void onFoundBluetoothDevice(Ptr<BluetoothDevice> bluetooth_device) override;
    void onLostBluetoothDevice(Ptr<BluetoothDevice> bluetooth_device) override;

   private:
    class OnFoundBluetoothDeviceRunnable : public Runnable {
     public:
      OnFoundBluetoothDeviceRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<FoundBluetoothAdvertisementProcessor>
              found_bluetooth_advertisement_processor,
          const string& service_id, Ptr<BluetoothDevice> bluetooth_device);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<FoundBluetoothAdvertisementProcessor>
          found_bluetooth_advertisement_processor_;
      const string service_id_;
      ScopedPtr<Ptr<BluetoothDevice>> bluetooth_device_;
    };

    class OnLostBluetoothDeviceRunnable : public Runnable {
     public:
      OnLostBluetoothDeviceRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<FoundBluetoothAdvertisementProcessor>
              found_bluetooth_advertisement_processor,
          const string& service_id, Ptr<BluetoothDevice> bluetooth_device);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<FoundBluetoothAdvertisementProcessor>
          found_bluetooth_advertisement_processor_;
      const string service_id_;
      ScopedPtr<Ptr<BluetoothDevice>> bluetooth_device_;
    };

    bool isRecognizedBluetoothEndpoint(
        const string& found_bluetooth_device_name,
        Ptr<BluetoothDeviceName> bluetooth_device_name);

    Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
    Ptr<ClientProxy<Platform>> client_proxy_;
    const string service_id_;
    ScopedPtr<ConstPtr<ByteArray>> expected_service_id_hash_;
    std::shared_ptr<FoundBluetoothAdvertisementProcessor> self_{this,
                                                                [](void*) {}};
  };

  class FoundBleAdvertisementProcessor
      : public MediumManager<Platform>::FoundBlePeripheralProcessor {
   public:
    FoundBleAdvertisementProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy);

    void onFoundBlePeripheral(Ptr<BLE_PERIPHERAL> ble_peripheral,
                              const string& service_id,
                              ConstPtr<ByteArray> advertisement_bytes) override;
    void onLostBlePeripheral(Ptr<BLE_PERIPHERAL> ble_peripheral,
                             const string& service_id) override;

   private:
    class OnFoundBlePeripheralRunnable : public Runnable {
     public:
      OnFoundBlePeripheralRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<FoundBleAdvertisementProcessor> found_ble_advertisement_processor,
          const string& service_id, Ptr<BLE_PERIPHERAL> ble_peripheral,
          ConstPtr<ByteArray> advertisement_bytes);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<FoundBleAdvertisementProcessor> found_ble_advertisement_processor_;
      const string service_id_;
      ScopedPtr<Ptr<BLE_PERIPHERAL>> ble_peripheral_;
      ScopedPtr<ConstPtr<ByteArray>> advertisement_bytes_;
      ScopedPtr<ConstPtr<ByteArray>> expected_service_id_hash_;
    };

    class OnLostBlePeripheralRunnable : public Runnable {
     public:
      OnLostBlePeripheralRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<FoundBleAdvertisementProcessor> found_ble_advertisement_processor,
          const string& service_id, Ptr<BLE_PERIPHERAL> ble_peripheral);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<FoundBleAdvertisementProcessor> found_ble_advertisement_processor_;
      const string service_id_;
      ScopedPtr<Ptr<BLE_PERIPHERAL>> ble_peripheral_;
    };

    // Holds the state required to re-create a BLEEndpoint we see on a
    // BLEPeripheral, so OnLostBlePeripheralRunnable::run() can call
    // BasePCPHandler::onEndpointLost() with the same information as was passed
    // in to BasePCPHandler::onEndpointFound().
    struct BLEEndpointState {
     public:
      BLEEndpointState(const string& endpoint_id, const string& endpoint_name)
          : endpoint_id(endpoint_id), endpoint_name(endpoint_name) {}

      const string endpoint_id;
      const string endpoint_name;
    };

    Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
    Ptr<ClientProxy<Platform>> client_proxy_;

    // Maps a BLEPeripheral to its corresponding BLEEndpointState.
    typedef std::map<string, BLEEndpointState> FoundBLEEndpointsMap;
    FoundBLEEndpointsMap found_ble_endpoints_;
    std::shared_ptr<FoundBleAdvertisementProcessor> self_{this, [](void*) {}};
  };

  class FoundWifiLanServiceProcessor
      : public MediumManager<Platform>::FoundWifiLanServiceProcessor {
   public:
    FoundWifiLanServiceProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        absl::string_view service_id);

    void OnFoundWifiLanService(Ptr<WifiLanService> wifi_lan_service) override;
    void OnLostWifiLanService(Ptr<WifiLanService> wifi_lan_service) override;

   private:
    class OnFoundWifiLanServiceRunnable : public Runnable {
     public:
      OnFoundWifiLanServiceRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<FoundWifiLanServiceProcessor> found_wifi_lan_service_processor,
          absl::string_view service_id, Ptr<WifiLanService> wifi_lan_service);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<FoundWifiLanServiceProcessor> found_wifi_lan_service_processor_;
      const string service_id_;
      ScopedPtr<Ptr<WifiLanService>> wifi_lan_service_;
      ScopedPtr<ConstPtr<ByteArray>> expected_service_id_hash_;
    };

    class OnLostWifiLanServiceRunnable : public Runnable {
     public:
      OnLostWifiLanServiceRunnable(
          Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
          Ptr<ClientProxy<Platform>> client_proxy,
          Ptr<FoundWifiLanServiceProcessor> found_wifi_lan_service_processor,
          absl::string_view service_id, Ptr<WifiLanService> wifi_lan_service);

      void run() override;

     private:
      Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
      Ptr<ClientProxy<Platform>> client_proxy_;
      Ptr<FoundWifiLanServiceProcessor> found_wifi_lan_service_processor_;
      const string service_id_;
      ScopedPtr<Ptr<WifiLanService>> wifi_lan_service_;
    };

    bool IsRecognizedWifiLanEndpoint(
        Ptr<WifiLanServiceInfo> wifi_lan_service_info);

    Ptr<P2PClusterPCPHandler<Platform>> pcp_handler_;
    Ptr<ClientProxy<Platform>> client_proxy_;
    const string service_id_;
    ScopedPtr<ConstPtr<ByteArray>> expected_service_id_hash_;
    std::shared_ptr<FoundWifiLanServiceProcessor> self_{this, [](void*) {}};
  };

  class BluetoothEndpoint
      : public BasePCPHandler<Platform>::DiscoveredEndpoint {
   public:
    Ptr<BluetoothDevice> getBluetoothDevice() {
      return bluetooth_device_.get();
    }
    string getEndpointId() override { return endpoint_id_; }
    string getEndpointName() override { return endpoint_name_; }
    string getServiceId() override { return service_id_; }
    proto::connections::Medium getMedium() override {
      return proto::connections::Medium::BLUETOOTH;
    }

   private:
    BluetoothEndpoint(Ptr<BluetoothDevice> bluetooth_device,
                      const string& endpoint_id, const string& endpoint_name,
                      const string& service_id)
        : bluetooth_device_(bluetooth_device),
          endpoint_id_(endpoint_id),
          endpoint_name_(endpoint_name),
          service_id_(service_id) {}

    friend class FoundBluetoothAdvertisementProcessor;

    ScopedPtr<Ptr<BluetoothDevice>> bluetooth_device_;
    const string endpoint_id_;
    const string endpoint_name_;
    const string service_id_;
  };

  class BLEEndpoint : public BasePCPHandler<Platform>::DiscoveredEndpoint {
   public:
    Ptr<BLE_PERIPHERAL> getBlePeripheral() { return ble_peripheral_.get(); }
    string getEndpointId() override { return endpoint_id_; }
    string getEndpointName() override { return endpoint_name_; }
    string getServiceId() override { return service_id_; }
    proto::connections::Medium getMedium() override {
      return proto::connections::Medium::BLE;
    }

   private:
    BLEEndpoint(Ptr<BLE_PERIPHERAL> ble_peripheral, const string& endpoint_id,
                const string& endpoint_name, const string& service_id)
        : ble_peripheral_(ble_peripheral),
          endpoint_id_(endpoint_id),
          endpoint_name_(endpoint_name),
          service_id_(service_id) {}

    friend class FoundBleAdvertisementProcessor;

    ScopedPtr<Ptr<BLE_PERIPHERAL>> ble_peripheral_;
    const string endpoint_id_;
    const string endpoint_name_;
    const string service_id_;
  };

  class WifiLanEndpoint : public BasePCPHandler<Platform>::DiscoveredEndpoint {
   public:
    Ptr<WifiLanService> GetWifiLanService() { return wifi_lan_service_.get(); }
    string getEndpointId() override { return endpoint_id_; }
    string getEndpointName() override { return endpoint_name_; }
    string getServiceId() override { return service_id_; }
    proto::connections::Medium getMedium() override {
      return proto::connections::Medium::WIFI_LAN;
    }

   private:
    WifiLanEndpoint(Ptr<WifiLanService> wifi_lan_service,
                    absl::string_view endpoint_id,
                    absl::string_view endpoint_name,
                    absl::string_view service_id)
        : wifi_lan_service_(wifi_lan_service),
          endpoint_id_(endpoint_id),
          endpoint_name_(endpoint_name),
          service_id_(service_id) {}

    friend class FoundWifiLanServiceProcessor;

    ScopedPtr<Ptr<WifiLanService>> wifi_lan_service_;
    const string endpoint_id_;
    const string endpoint_name_;
    const string service_id_;
  };

  static const BluetoothDeviceName::Version::Value kBluetoothDeviceNameVersion;
  static const BLEAdvertisement::Version::Value kBleAdvertisementVersion;
  static const WifiLanServiceInfo::Version kWifiLanServiceInfoVersion;

  static ConstPtr<ByteArray> generateHash(const string& source, size_t size);
  static string getBlePeripheralId(Ptr<BLE_PERIPHERAL> ble_peripheral);

  proto::connections::Medium startBluetoothAdvertising(
      Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
      ConstPtr<ByteArray> service_id_hash, const string& local_endpoint_id,
      const string& local_endpoint_name);
  proto::connections::Medium startBluetoothDiscovery(
      Ptr<FoundBluetoothAdvertisementProcessor> processor,
      Ptr<ClientProxy<Platform>> client_proxy, const string& service_id);
  typename BasePCPHandler<Platform>::ConnectImplResult bluetoothConnectImpl(
      Ptr<ClientProxy<Platform>> client_proxy,
      Ptr<BluetoothEndpoint> bluetooth_endpoint);

  proto::connections::Medium startBleAdvertising(
      Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
      ConstPtr<ByteArray> service_id_hash, const string& local_endpoint_id,
      const string& local_endpoint_name);
  proto::connections::Medium startBleDiscovery(
      Ptr<FoundBleAdvertisementProcessor> processor,
      Ptr<ClientProxy<Platform>> client_proxy, const string& service_id);
  typename BasePCPHandler<Platform>::ConnectImplResult bleConnectImpl(
      Ptr<ClientProxy<Platform>> client_proxy, Ptr<BLEEndpoint> ble_endpoint);

  proto::connections::Medium StartWifiLanAdvertising(
      Ptr<ClientProxy<Platform>> client_proxy, absl::string_view service_id,
      ConstPtr<ByteArray> service_id_hash, absl::string_view local_endpoint_id,
      absl::string_view local_endpoint_name);
  proto::connections::Medium StartWifiLanDiscovery(
      Ptr<FoundWifiLanServiceProcessor> processor,
      Ptr<ClientProxy<Platform>> client_proxy, absl::string_view service_id);
  typename BasePCPHandler<Platform>::ConnectImplResult WifiLanConnectImpl(
      Ptr<ClientProxy<Platform>> client_proxy,
      Ptr<WifiLanEndpoint> wifi_lan_endpoint);

  Ptr<MediumManager<Platform>> medium_manager_;
  std::shared_ptr<P2PClusterPCPHandler> self_{this, [](void*) {}};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/p2p_cluster_pcp_handler.cc"

#endif  // CORE_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
