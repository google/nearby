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

#ifndef PLATFORM_BASE_MEDIUM_ENVIRONMENT_H_
#define PLATFORM_BASE_MEDIUM_ENVIRONMENT_H_

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/base/observer_list.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/uuid.h"
#include "internal/test/fake_clock.h"
#ifndef NO_WEBRTC
#include "internal/platform/implementation/webrtc.h"
#endif
#include "internal/platform/byte_array.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/mutex.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {

// Environment config that can control availability of certain mediums for
// testing.
struct EnvironmentConfig {
  // Control whether WEB_RTC medium is enabled in the environment.
  // This is currently set to false, due to http://b/139734036 that would lead
  // to flaky tests.
  bool webrtc_enabled = false;

  // Installs a simulated clock, which can be used to test timeouts.
  // The simulated clock is automatically picked up by SystemClock, Timer and
  // ScheduledExecutor implementations.
  bool use_simulated_clock = false;
};

// MediumEnvironment is a simulated environment which allows multiple instances
// of simulated HW devices to "work" together as if they are physical.
// For each medium type it provides necessary methods to implement
// advertising, discovery and establishment of a data link.
// NOTE: this code depends on public:types target.
class MediumEnvironment {
 public:
  using BluetoothDiscoveryCallback =
      api::BluetoothClassicMedium::DiscoveryCallback;
  using BleDiscoveredPeripheralCallback =
      api::BleMedium::DiscoveredPeripheralCallback;
  using BleAcceptedConnectionCallback =
      api::BleMedium::AcceptedConnectionCallback;
  using BleScanCallback = api::ble_v2::BleMedium::ScanningCallback;
#ifndef NO_WEBRTC
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;
  using OnSignalingCompleteCallback =
      api::WebRtcSignalingMessenger::OnSignalingCompleteCallback;
#endif
  using WifiLanDiscoveredServiceCallback =
      api::WifiLanMedium::DiscoveredServiceCallback;

  struct BleV2MediumStatus {
    bool is_advertising;
    bool is_scanning;
  };

  MediumEnvironment(const MediumEnvironment&) = delete;
  MediumEnvironment& operator=(const MediumEnvironment&) = delete;

  // Creates and returns a reference to the global test environment instance.
  static MediumEnvironment& Instance();

  // Global ON/OFF switch for medium environment.
  // Start & Stop work as On/Off switch for this object.
  // Default state (after creation) is ON, to make it compatible with early
  // tests that are already using it and relying on it being ON.

  // Enables Medium environment.
  void Start(EnvironmentConfig config = EnvironmentConfig());
  // Disables Medium environment.
  void Stop();

  // Clears state. No notifications are sent.
  void Reset();

  // Waits for all previously scheduled jobs to finish.
  // This method works as a barrier that guarantees that after it returns, all
  // the activities that started before it was called, or while it was running
  // are ended. This means that system is at the state of relaxation when this
  // code returns. It requires external stimulus to get out of relaxation state.
  //
  // If enable_notifications is true (default), simulation environment
  // will send all future notification events to all registered objects,
  // whenever protocol requires that. This is expected behavior.
  // If enabled_notifications is false, future event notifications will not be
  // sent to registered instances. This is useful for protocol shutdown,
  // where we no longer care about notifications, and where notifications may
  // otherwise be delivered after the notification source or target lifetime has
  // ended, and cause undefined behavior.
  void Sync(bool enable_notifications = true);

  // Adds an adapter to internal container.
  // Notify BluetoothClassicMediums if any that adapter state has changed.
  void OnBluetoothAdapterChangedState(api::BluetoothAdapter& adapter,
                                      api::BluetoothDevice& adapter_device,
                                      std::string name, bool enabled,
                                      api::BluetoothAdapter::ScanMode mode);

  // Adds medium-related info to allow for adapter discovery to work.
  // This provides access to this medium from other mediums, when protocol
  // expects they should communicate.
  void RegisterBluetoothMedium(api::BluetoothClassicMedium& medium,
                               api::BluetoothAdapter& medium_adapter);

  // Updates discovery callback info to allow for dispatch of discovery events.
  //
  // Invokes callback asynchronously when any changes happen to discoverable
  // devices if it is turned on.
  //
  // This should be called when discoverable state changes.
  // A valid callback should be assigned when discovery `enabled` as true; or
  // an empty callback is assigned with discovery `enabled` as false.
  void UpdateBluetoothMedium(api::BluetoothClassicMedium& medium,
                             BluetoothDiscoveryCallback callback);

  // Removes medium-related info. This should correspond to device power off.
  void UnregisterBluetoothMedium(api::BluetoothClassicMedium& medium);

  // Returns a Bluetooth Device object matching given mac address to nullptr.
  api::BluetoothDevice* FindBluetoothDevice(const std::string& mac_address);

  api::ble_v2::BlePeripheral* FindBleV2Peripheral(
      absl::string_view mac_address);

  const EnvironmentConfig& GetEnvironmentConfig();
#ifndef NO_WEBRTC
  // Registers |message_callback| to receive messages sent to device with id
  // |self_id|, and |complete_callback| to notify when signaling is complete.
  void RegisterWebRtcSignalingMessenger(
      absl::string_view self_id, OnSignalingMessageCallback message_callback,
      OnSignalingCompleteCallback complete_callback);

  // Unregisters the callback listening to incoming messages for |self_id|.
  void UnregisterWebRtcSignalingMessenger(absl::string_view self_id);

  // Simulates sending a signaling message |message| to device with id
  // |peer_id|.
  void SendWebRtcSignalingMessage(absl::string_view peer_id,
                                  const ByteArray& message);

  // Simulates sending an "signaling complete" signal to the WebRTC medium.
  void SendWebRtcSignalingComplete(absl::string_view peer_id, bool success);
#endif
  // Used to set if WebRtcMedium should use a valid peer connection or nullptr
  // in tests.
  void SetUseValidPeerConnection(bool use_valid_peer_connection);

  bool GetUseValidPeerConnection();

  // Used to set latency when creating the peer connection in tests.
  void SetPeerConnectionLatency(absl::Duration peer_connection_latency);

  absl::Duration GetPeerConnectionLatency();

  // Adds medium-related info to allow for scanning/advertising to work.
  // This provides access to this medium from other mediums, when protocol
  // expects they should communicate.
  void RegisterBleMedium(api::BleMedium& medium);

  // Updates advertising info to indicate the current medium is exposing
  // advertising event.
  void UpdateBleMediumForAdvertising(api::BleMedium& medium,
                                     api::BlePeripheral& peripheral,
                                     const std::string& service_id,
                                     bool fast_advertisement, bool enabled);

  // Updates discovery callback info to allow for dispatch of discovery events.
  //
  // Invokes callback asynchronously when any changes happen to discoverable
  // devices if it is turned on.
  //
  // This should be called when discoverable state changes.
  // A valid callback should be assigned when discovery `enabled` as true; or
  // an empty callback is assigned with discovery `enabled` as false.
  void UpdateBleMediumForScanning(
      api::BleMedium& medium, const std::string& service_id,
      const std::string& fast_advertisement_service_uuid,
      BleDiscoveredPeripheralCallback callback, bool enabled);

  // Updates Accepted connection callback info to allow for dispatch of
  // advertising events.
  void UpdateBleMediumForAcceptedConnection(
      api::BleMedium& medium, const std::string& service_id,
      BleAcceptedConnectionCallback callback);

  // Removes medium-related info. This should correspond to device power off.
  void UnregisterBleMedium(api::BleMedium& medium);

  // Call back when advertising has created the server socket and is ready for
  // connect.
  void CallBleAcceptedConnectionCallback(api::BleMedium& medium,
                                         api::BleSocket& socket,
                                         const std::string& service_id);

  // Adds medium-related info to allow for scanning/advertising to work.
  // This provides access to this medium from other mediums, when protocol
  // expects they should communicate.
  // The registered `medium` must refer to a valid instance that outlives this
  // object.
  void RegisterBleV2Medium(api::ble_v2::BleMedium& medium,
                           api::ble_v2::BlePeripheral* peripheral);

  // Updates advertising info to indicate the current medium is exposing
  // advertising event.
  void UpdateBleV2MediumForAdvertising(
      bool enabled, api::ble_v2::BleMedium& medium,
      api::ble_v2::BlePeripheral& peripheral,
      const api::ble_v2::BleAdvertisementData& advertisement_data);

  // Updates discovery callback info to allow for dispatch of discovery events.
  //
  // Invokes callback asynchronously when any changes happen to discoverable
  // devices if it is turned on.
  //
  // This should be called when discoverable state changes.
  // The `callback` argument should be non-empty if `enabled` is true or empty
  // if `enabled` is false.
  void UpdateBleV2MediumForScanning(bool enabled,
                                    const Uuid& scanning_service_uuid,
                                    std::uint32_t internal_session_id,
                                    BleScanCallback callback,
                                    api::ble_v2::BleMedium& medium);

  // Removes medium-related info. This should correspond to device power off.
  void UnregisterBleV2Medium(api::ble_v2::BleMedium& mediumum);

  // Collects the status for the given BleMedium. Mainly used in unit tests
  // to verify if the BleMedum is in expected status after opeartions.
  absl::optional<BleV2MediumStatus> GetBleV2MediumStatus(
      const api::ble_v2::BleMedium& medium);

  // Adds medium-related info to allow for discovery/advertising to work.
  // This provides access to this medium from other mediums, when protocol
  // expects they should communicate.
  void RegisterWifiLanMedium(api::WifiLanMedium& medium);

  // Updates advertising info to indicate the current medium is exposing
  // advertising event.
  void UpdateWifiLanMediumForAdvertising(api::WifiLanMedium& medium,
                                         const NsdServiceInfo& nsd_service_info,
                                         bool enabled);

  // Updates discovery callback info to allow for dispatch of discovery events.
  //
  // This should be called when discoverable state changes.
  // A valid callback should be assigned when discovery `enabled` as true; or
  // an empty callback is assigned with discovery `enabled` as false.
  void UpdateWifiLanMediumForDiscovery(
      api::WifiLanMedium& medium, WifiLanDiscoveredServiceCallback callback,
      const std::string& service_type, bool enabled);

  // Gets Fake IP address for WifiLan medium.
  std::string GetFakeIPAddress() const;

  // Gets Fake port number for WifiLan medium.
  int GetFakePort() const;

  // Removes medium-related info. This should correspond to device power off.
  void UnregisterWifiLanMedium(api::WifiLanMedium& medium);

  // Returns WifiLan medium whose advertising service matching IP address and
  // port, or nullptr.
  api::WifiLanMedium* GetWifiLanMedium(const std::string& ip_address, int port);

  // Adds medium-related info to allow for start/connect WifiDirect to work.
  void RegisterWifiDirectMedium(api::WifiDirectMedium& medium);

  // Returns WifiDirect medium that matches ssid or IP address with the role of
  // the Medium. Returns nullptr if not found.
  api::WifiDirectMedium* GetWifiDirectMedium(absl::string_view ssid,
                                             absl::string_view ip_address);

  // Updates credential and Medium role(GO or GC) to indicate the current
  // medium is exposing Start WifiDirect event.
  void UpdateWifiDirectMediumForStartOrConnect(
      api::WifiDirectMedium& medium,
      const WifiDirectCredentials* wifi_direct_credentials, bool is_go,
      bool enabled);

  // For unit test only
  bool IsWifiDirectMediumsEmpty();

  // Removes medium-related info. This should correspond to device being stopped
  // or disconnected.
  void UnregisterWifiDirectMedium(api::WifiDirectMedium& medium);

  // Adds medium-related info to allow for start/connect Hotspot to work.
  // This provides access to this medium from other mediums, when protocol
  // expects they should communicate.
  void RegisterWifiHotspotMedium(api::WifiHotspotMedium& medium);

  // Returns WifiSpot medium that matches ssid or IP address with the role of
  // the Medium, or return nullptr.
  api::WifiHotspotMedium* GetWifiHotspotMedium(absl::string_view ssid,
                                               absl::string_view ip_address);

  // Updates credential and Medium role(AP or STA) to indicate the current
  // medium is exposing Start Hotspot event.
  void UpdateWifiHotspotMediumForStartOrConnect(
      api::WifiHotspotMedium& medium, HotspotCredentials* hotspot_credentials,
      bool is_ap, bool enabled);

  // Removes medium-related info. This should correspond to device stopped or
  // disconnected.
  void UnregisterWifiHotspotMedium(api::WifiHotspotMedium& medium);

  void SetFeatureFlags(const FeatureFlags::Flags& flags);

  absl::optional<FakeClock*> GetSimulatedClock();

  api::ble_v2::BleMedium* FindBleV2Medium(absl::string_view address);
  api::ble_v2::BleMedium* FindBleV2Medium(uint64_t id);

  void RegisterGattServer(api::ble_v2::BleMedium& medium,
                          api::ble_v2::BlePeripheral* peripheral,
                          Borrowable<api::ble_v2::GattServer*> gatt_server);
  void UnregisterGattServer(api::ble_v2::BleMedium& medium);

  Borrowable<api::ble_v2::GattServer*> GetGattServer(
      api::ble_v2::BlePeripheral& peripheral);

  // Configures the BluetoothPairingContext for remote BluetoothDevice.
  void ConfigBluetoothPairingContext(api::BluetoothDevice* device,
                                     api::PairingParams pairing_params);

  // Sets the current pairing states of remote BluetoothDevice.
  bool SetPairingState(api::BluetoothDevice* device, bool paired);

  // Mocks the pairing result for remote BluetoothDevice.
  bool SetPairingResult(
      api::BluetoothDevice* device,
      std::optional<api::BluetoothPairingCallback::PairingError> error);

  // Requests for pairing with remote BluetoothDevice,
  // and registers BluetoothPairingCallback.
  bool InitiatePairing(api::BluetoothDevice* remote_device,
                       api::BluetoothPairingCallback pairing_cb);

  // Finishes pairing for remote BluetoothDevice.
  bool FinishPairing(api::BluetoothDevice* device);

  // Cancels ongoing pairing for remote BluetoothDevice.
  bool CancelPairing(api::BluetoothDevice* device);

  // Returns the pairing states of remote BluetoothDevice.
  bool IsPaired(api::BluetoothDevice* device);

  // Clears the map `devices_pairing_contexts_`.
  void ClearBluetoothDevicesForPairing();

  void AddObserver(api::BluetoothClassicMedium::Observer* observer);
  void RemoveObserver(api::BluetoothClassicMedium::Observer* observer);

 private:
  struct BluetoothMediumContext {
    BluetoothDiscoveryCallback callback;
    api::BluetoothAdapter* adapter = nullptr;
    // discovered device vs device name map.
    absl::flat_hash_map<api::BluetoothDevice*, std::string> devices;
  };

  struct BleMediumContext {
    BleDiscoveredPeripheralCallback discovery_callback;
    BleAcceptedConnectionCallback accepted_connection_callback;
    api::BlePeripheral* ble_peripheral = nullptr;
    bool advertising = false;
    bool fast_advertisement = false;
  };

  struct BleV2MediumContext {
    absl::flat_hash_map<std::pair<Uuid, std::uint32_t>, BleScanCallback>
        scan_callback_map;
    // using the same ble peripheral for different advertisement.
    api::ble_v2::BlePeripheral* ble_peripheral;
    api::ble_v2::BleAdvertisementData advertisement_data;
    bool advertising = false;
    bool scanning = false;
    std::unique_ptr<Borrowable<api::ble_v2::GattServer*>> gatt_server = nullptr;
  };

  struct WifiLanMediumContext {
    // advertising service type vs NsdServiceInfo map.
    absl::flat_hash_map<std::string, NsdServiceInfo> advertising_services;
    // discovered service type vs callback map.
    absl::flat_hash_map<std::string, WifiLanDiscoveredServiceCallback>
        discovered_callbacks;
    // discovered service vs service type map.
    absl::flat_hash_map<std::string, NsdServiceInfo> discovered_services;
  };

  struct WifiDirectMediumContext {
    // Set to "true" for Medium act as WifiDirect GO role; "false" for GC role
    bool is_go = false;
    // Set "true" when GO is started or GC is connected
    bool is_active = false;
    const WifiDirectCredentials* wifi_direct_credentials;
  };

  struct WifiHotspotMediumContext {
    // Set to "true" for Medium act as SoftAP role; "false" for STA role
    bool is_ap = true;
    // Set "true" when SoftAP is started or STA is connected
    bool is_active = false;
    HotspotCredentials* hotspot_credentials;
  };

  struct BluetoothPairingContext {
    api::PairingParams pairing_params;
    api::BluetoothPairingCallback pairing_callback;
    bool is_paired = false;
    std::optional<api::BluetoothPairingCallback::PairingError> pairing_error;
  };

  // This is a singleton object, for which destructor will never be called.
  // Constructor will be invoked once from Instance() static method.
  // Object is create in-place (with a placement new) to guarantee that
  // destructor is not scheduled for execution at exit.
  MediumEnvironment() = default;
  ~MediumEnvironment() = default;

  void OnBluetoothDeviceStateChanged(BluetoothMediumContext& info,
                                     api::BluetoothDevice& device,
                                     const std::string& name,
                                     api::BluetoothAdapter::ScanMode mode,
                                     bool enabled);

  void OnBlePeripheralStateChanged(BleMediumContext& info,
                                   api::BlePeripheral& peripheral,
                                   const std::string& service_id,
                                   bool fast_advertisement, bool enabled);

  void OnBleV2PeripheralStateChanged(
      bool enabled, BleV2MediumContext& context, const Uuid& service_id,
      const api::ble_v2::BleAdvertisementData& ble_advertisement_data,
      api::ble_v2::BlePeripheral& peripheral);

  void OnWifiLanServiceStateChanged(WifiLanMediumContext& info,
                                    const NsdServiceInfo& service_info,
                                    bool enabled);

  void RunOnMediumEnvironmentThread(Runnable runnable);

  std::atomic_bool enabled_ = false;
  std::atomic_int job_count_ = 0;
  std::atomic_bool enable_notifications_ = false;
  SingleThreadExecutor executor_;
  EnvironmentConfig config_;

  // The following data members are accessed in the context of a private
  // executor_ thread.
  absl::flat_hash_map<api::BluetoothAdapter*, api::BluetoothDevice*>
      bluetooth_adapters_;
  absl::flat_hash_map<api::BluetoothClassicMedium*, BluetoothMediumContext>
      bluetooth_mediums_;

  absl::flat_hash_map<api::BleMedium*, BleMediumContext> ble_mediums_;
  absl::flat_hash_map<api::ble_v2::BleMedium*, BleV2MediumContext>
      ble_v2_mediums_;
  absl::flat_hash_map<api::BluetoothDevice*, BluetoothPairingContext>
      devices_pairing_contexts_;
#ifndef NO_WEBRTC
  // Maps peer id to callback for receiving signaling messages.
  absl::flat_hash_map<std::string, OnSignalingMessageCallback>
      webrtc_signaling_message_callback_;

  // Maps peer id to callback for signaling complete events.
  absl::flat_hash_map<std::string, OnSignalingCompleteCallback>
      webrtc_signaling_complete_callback_;
#endif

  absl::flat_hash_map<api::WifiLanMedium*, WifiLanMediumContext>
      wifi_lan_mediums_;

  Mutex mutex_;
  absl::flat_hash_map<api::WifiDirectMedium*, WifiDirectMediumContext>
      wifi_direct_mediums_ ABSL_GUARDED_BY(mutex_);

  absl::flat_hash_map<api::WifiHotspotMedium*, WifiHotspotMediumContext>
      wifi_hotspot_mediums_ ABSL_GUARDED_BY(mutex_);

  bool use_valid_peer_connection_ = true;
  absl::Duration peer_connection_latency_ = absl::ZeroDuration();
  std::unique_ptr<FakeClock> simulated_clock_ ABSL_GUARDED_BY(mutex_);
  ObserverList<api::BluetoothClassicMedium::Observer> observers_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_MEDIUM_ENVIRONMENT_H_
