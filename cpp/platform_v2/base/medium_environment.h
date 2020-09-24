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

#ifndef PLATFORM_V2_BASE_MEDIUM_ENVIRONMENT_H_
#define PLATFORM_V2_BASE_MEDIUM_ENVIRONMENT_H_

#include <atomic>

#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/api/webrtc.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/listeners.h"
#include "platform_v2/public/single_thread_executor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

// Environment config that can control availability of certain mediums for
// testing.
struct EnvironmentConfig {
  // Control whether WEB_RTC medium is enabled in the environment.
  // This is currently set to false, due to http://b/139734036 that would lead
  // to flaky tests.
  bool webrtc_enabled = false;
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
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;
  using WifiLanDiscoveredServiceCallback =
      api::WifiLanMedium::DiscoveredServiceCallback;
  using WifiLanAcceptedConnectionCallback =
      api::WifiLanMedium::AcceptedConnectionCallback;

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
  // otherwise be delivered after the notification source or target lifeteme has
  // ended, and cause undefined behavior.
  void Sync(bool enable_notifications = true);

  // Adds an adapter to internal container.
  // Notify BluetoothClassicMediums if any that adapter state has changed.
  void OnBluetoothAdapterChangedState(api::BluetoothAdapter& adapter,
                                      api::BluetoothDevice& adapter_device,
                                      std::string name, bool enabled,
                                      api::BluetoothAdapter::ScanMode mode);

  // Adds medium-related info to allow for adapter discovery to work.
  // This provides acccess to this medium from other mediums, when protocol
  // expects they should communicate.
  void RegisterBluetoothMedium(api::BluetoothClassicMedium& medium,
                               api::BluetoothAdapter& medium_adapter);

  // Updates callback info to allow for dispatch of discovery events.
  //
  // Invokes callback asynchronously when any changes happen to discoverable
  // devices, or if the defice is turned off, whether or not it is discoverable,
  // if it was ever reported as discoverable.
  //
  // This should be called when discoverable state changes.
  // with user-specified callback when discovery is enabled, and with default
  // (empty) callback otherwise.
  void UpdateBluetoothMedium(api::BluetoothClassicMedium& medium,
                             BluetoothDiscoveryCallback callback);

  // Removes medium-related info. This should correspond to device power off.
  void UnregisterBluetoothMedium(api::BluetoothClassicMedium& medium);

  // Returns a Bluetooth Device object matching given mac address to nullptr.
  api::BluetoothDevice* FindBluetoothDevice(const std::string& mac_address);

  const EnvironmentConfig& GetEnvironmentConfig();

  // Registers |callback| to receive messages sent to device with id |self_id|.
  void RegisterWebRtcSignalingMessenger(absl::string_view self_id,
                                        OnSignalingMessageCallback callback);

  // Unregisters the callback listening to incoming messages for |self_id|.
  void UnregisterWebRtcSignalingMessenger(absl::string_view self_id);

  // Simulates sending a signaling message |message| to device with id
  // |peer_id|.
  void SendWebRtcSignalingMessage(absl::string_view peer_id,
                                  const ByteArray& message);

  // Used to set if WebRtcMedium should use a valid peer connection or nullptr
  // in tests.
  void SetUseValidPeerConnection(bool use_valid_peer_connection);

  bool GetUseValidPeerConnection();

  // Adds medium-related info to allow for scanning/advertising to work.
  // This provides acccess to this medium from other mediums, when protocol
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
  // devices, or if the defice is turned off, whether or not it is discoverable,
  // if it was ever reported as discoverable.
  //
  // This should be called when discoverable state changes.
  // with user-specified callback when discovery is enabled, and with default
  // (empty) callback otherwise.
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

  // Adds medium-related info to allow for discovery/advertising to work.
  // This provides acccess to this medium from other mediums, when protocol
  // expects they should communicate.
  void RegisterWifiLanMedium(api::WifiLanMedium& medium);

  // Updates advertising info to indicate the current medium is exposing
  // advertising event.
  void UpdateWifiLanMediumForAdvertising(api::WifiLanMedium& medium,
                                         api::WifiLanService& service,
                                         const std::string& service_id,
                                         bool enabled);

  // Updates discovery callback info to allow for dispatch of discovery events.
  //
  // Invokes callback asynchronously when any changes happen to discoverable
  // devices, or if the defice is turned off, whether or not it is discoverable,
  // if it was ever reported as discoverable.
  //
  // This should be called when discoverable state changes.
  // with user-specified callback when discovery is enabled, and with default
  // (empty) callback otherwise.
  void UpdateWifiLanMediumForDiscovery(
      api::WifiLanMedium& medium, const std::string& service_id,
      WifiLanDiscoveredServiceCallback callback, bool enabled);

  // Updates Accepted connection callback info to allow for dispatch of
  // advertising events.
  void UpdateWifiLanMediumForAcceptedConnection(
      api::WifiLanMedium& medium, const std::string& service_id,
      WifiLanAcceptedConnectionCallback callback);

  // Removes medium-related info. This should correspond to device power off.
  void UnregisterWifiLanMedium(api::WifiLanMedium& medium);

  // Call back when advertising has created the server socket and is ready for
  // connect.
  void CallWifiLanAcceptedConnectionCallback(api::WifiLanMedium& medium,
                                             api::WifiLanSocket& socket,
                                             const std::string& service_id);

  // Returns WiFi LAN service matching IP address and port, or nullptr.
  api::WifiLanService* FindWifiLanService(const std::string& ip_address,
                                          int port);

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

  struct WifiLanServiceIdContext {
    WifiLanDiscoveredServiceCallback discovery_callback;
    WifiLanAcceptedConnectionCallback accepted_connection_callback;
    bool advertising = false;
  };

  struct WifiLanMediumContext {
    api::WifiLanService* wifi_lan_service = nullptr;
    absl::flat_hash_map<std::string, WifiLanServiceIdContext> services;
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

  void OnWifiLanServiceStateChanged(WifiLanMediumContext& info,
                                    api::WifiLanService& service,
                                    const std::string& service_id,
                                    bool enabled);

  void RunOnMediumEnvironmentThread(std::function<void()> runnable);

  std::atomic_bool enabled_ = true;
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

  // Maps peer id to callback for receiving signaling messages.
  absl::flat_hash_map<std::string, OnSignalingMessageCallback>
      webrtc_signaling_callback_;

  absl::flat_hash_map<api::WifiLanMedium*, WifiLanMediumContext>
      wifi_lan_mediums_;

  bool use_valid_peer_connection_ = true;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_BASE_MEDIUM_ENVIRONMENT_H_
