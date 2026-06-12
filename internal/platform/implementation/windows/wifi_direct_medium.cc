// Copyright 2025 Google LLC
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

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/windows/device_info.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {

namespace {
constexpr absl::Duration kServiceConnectionTimeout = absl::Seconds(60);
constexpr absl::Duration kWaitingForRePair = absl::Seconds(3);
constexpr absl::Duration kWaitForGOServerStart = absl::Milliseconds(500);
constexpr absl::Duration kConnectTimeout = absl::Seconds(30);
}  // namespace

WifiDirectDeviceDiscovered::WifiDirectDeviceDiscovered(
    const DeviceInformation& device_info)
    : windows_wifi_direct_device_(device_info) {
  id_ = winrt::to_string(device_info.Id());
}

// WifiDirectDeviceDiscovered::~WifiDirectDeviceDiscovered() {}
WifiDirectMedium::WifiDirectMedium() {
  is_interface_valid_ = IsWifiDirectSupported();
}

WifiDirectMedium::~WifiDirectMedium() {
  is_interface_valid_ = false;
  StopWifiDirect();
  DisconnectWifiDirect();
}

bool WifiDirectMedium::IsWifiDirectSupported() {
  HANDLE wifi_direct_handle = nullptr;
  DWORD negotiated_version = 0;
  DWORD result = 0;

  result =
      WFDOpenHandle(WFD_API_VERSION, &negotiated_version, &wifi_direct_handle);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "WiFi can't support WifiDirect";
    return false;
  }
  LOG(INFO) << "WiFi can support WifiDirect";
  WFDCloseHandle(wifi_direct_handle);
  return true;
}

bool WifiDirectMedium::IsInterfaceValid() const { return is_interface_valid_; }

// Discoverer connects to server socket
std::unique_ptr<api::WifiDirectSocket> WifiDirectMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << "WifiDirectMedium::ConnectToService Server Socket";
  // check current status
  if (!IsConnecting()) {
    LOG(WARNING) << "GC is not connecting to GO, skip.";
    return nullptr;
  }

  LOG(INFO) << "Remote gateway: " << ip_address << ", Port: " << port;

  std::string remote_ip_address = ip_address_remote_;

  if (remote_ip_address.empty() || port == 0) {
    LOG(ERROR) << "no valid service address and port to connect: "
               << "ip_address = " << remote_ip_address << ", port = " << port;
    return nullptr;
  }

  SocketAddress server_address;
  if (!SocketAddress::FromString(server_address, remote_ip_address, port)) {
    LOG(ERROR) << "no valid service address and port to connect.";
    return nullptr;
  }
  LOG(INFO) << "ConnectToService server address: " << server_address.ToString();

  // Try connecting to the service up to wifi_direct_max_connection_retries,
  // because it may fail first time if DHCP procedure is not finished yet.
  int64_t wifi_direct_max_connection_retries =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionMaxRetries);
  int64_t wifi_direct_retry_interval_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionIntervalMillis);
  int64_t wifi_direct_client_socket_connect_timeout_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionTimeoutMillis);

  LOG(INFO) << "maximum connection retries="
            << wifi_direct_max_connection_retries
            << ", connection interval=" << wifi_direct_retry_interval_millis
            << "ms, connection timeout="
            << wifi_direct_client_socket_connect_timeout_millis << "ms";

  LOG(INFO) << "Connect to service ";
  // In the test, GO server takes longer to started, so wait for 500ms before
  // trying to connect to the service.
  absl::SleepFor(kWaitForGOServerStart);
  for (int i = 0; i < wifi_direct_max_connection_retries; ++i) {
    auto wifi_direct_socket = std::make_unique<WifiDirectSocket>();

    // setup cancel listener
    std::unique_ptr<CancellationFlagListener> connection_cancellation_listener =
        nullptr;
    if (cancellation_flag != nullptr) {
      if (cancellation_flag->Cancelled()) {
        LOG(INFO) << "connect has been cancelled to service ";
        return nullptr;
      }

      connection_cancellation_listener =
          std::make_unique<nearby::CancellationFlagListener>(
              cancellation_flag, [socket = wifi_direct_socket.get()]() {
                LOG(WARNING) << "connect is closed due to it is cancelled.";
                socket->Close();
              });
    }

    bool result = wifi_direct_socket->Connect(server_address);
    if (result) {
      LOG(INFO) << "connected to remote service ";
      return wifi_direct_socket;
    } else {
      LOG(WARNING) << "reconnect to service at " << (i + 1) << "th times";
      Sleep(wifi_direct_retry_interval_millis);
    }
  }

  LOG(ERROR) << "Failed to connect to service ";
  return nullptr;
}

// Advertiser starts to listen on server socket
std::unique_ptr<api::WifiDirectServerSocket> WifiDirectMedium::ListenForService(
    int port) {
  LOG(INFO) << __func__
            << " :Start to listen connection from WiFiDirect client.";

  absl::MutexLock lock(mutex_);
  if (!IsBeaconing()) {
    LOG(WARNING) << "WifiDirect GO is not started, skip.";
    return nullptr;
  }
  // check current status
  if (IsAccepting()) {
    LOG(WARNING) << "Accepting connections already started on port "
                 << server_socket_ptr_->GetPort();
    return nullptr;
  }

  auto server_socket = std::make_unique<WifiDirectServerSocket>();
  server_socket_ptr_ = server_socket.get();

  // Start to listen on server socket in a separate thread. Before GC
  // connects to GO, GO doesn't have IP address. BWU calls this API right away
  // after it starts GO, we need to spin out the following logic to another
  // thread to avoid blocking BWU sending out of band upgrade frame to GC.
  listener_executor_.Execute([this, port]() mutable {
    absl::MutexLock lock(mutex_);
    if (ip_address_local_.empty()) {
      if (server_socket_ptr_) {
        LOG(INFO) << "Waiting for IP address is ready.";
        is_ip_address_ready_.WaitWithTimeout(&mutex_,
                                             kServiceConnectionTimeout);
        if (!server_socket_ptr_) {
          LOG(WARNING)
              << "Server socket was closed before IP address is ready.";
          return;
        }
        if (ip_address_local_.empty()) {
          LOG(WARNING) << "IP address is still empty, probably the GO doesn't "
                          "receive GC's connection request.";
          server_socket_ptr_->Close();
          server_socket_ptr_ = nullptr;
          return;
        }
        LOG(INFO) << "IP address is ready.";
      }
    }
    server_socket_ptr_->SetIPAddress(ip_address_local_);
    if (port == 0) {
      port = FeatureFlags::GetInstance().GetFlags().wifi_direct_default_port;
    }
    if (server_socket_ptr_ && server_socket_ptr_->Listen(port)) {
      medium_status_ |= kMediumStatusAccepting;

      // Setup close notifier after listen started.
      server_socket_ptr_->SetCloseNotifier([this]() {
        absl::MutexLock lock(mutex_);
        LOG(INFO) << "Server socket was closed.";
        medium_status_ &= (~kMediumStatusAccepting);
        server_socket_ptr_ = nullptr;
        is_ip_address_ready_.SignalAll();
      });
      LOG(INFO) << "Started to listen serive on port "
                << server_socket_ptr_->GetPort();
    } else {
      LOG(WARNING) << "server_socket_ptr_ is null or Listen failed.";
      if (server_socket_ptr_) {
        server_socket_ptr_->Close();
        server_socket_ptr_ = nullptr;
      }
    }
  });

  LOG(INFO) << "Started to listen service on port " << port;
  return server_socket;
}

bool WifiDirectMedium::StartWifiDirect(
    WifiDirectCredentials* wifi_direct_credentials) {
  absl::MutexLock lock(mutex_);
  LOG(INFO) << __func__ << ": Start to create WiFiDirect.";
  if (IsBeaconing()) {
    LOG(WARNING) << "Cannot create WiFiDirect GO again when it is running.";
    return true;
  }
  try {
    publisher_ = WiFiDirectAdvertisementPublisher();
    publisher_status_changed_token_ =
        publisher_.StatusChanged({this, &WifiDirectMedium::OnStatusChanged});
    listener_ = WiFiDirectConnectionListener();
    connection_requested_token_ = listener_.ConnectionRequested(
        {this, &WifiDirectMedium::OnConnectionRequested});
    // Normal mode: The device is highly discoverable so long as the app is in
    // the foreground.
    publisher_.Advertisement().ListenStateDiscoverability(
        WiFiDirectAdvertisementListenStateDiscoverability::Normal);
    // Enable Autonomous GO mode
    publisher_.Advertisement().IsAutonomousGroupOwnerEnabled(true);

    publisher_.Start();
    if (publisher_.Status() ==
        WiFiDirectAdvertisementPublisherStatus::Started) {
      LOG(INFO) << "Windows WIFI Direct AutoGO started";
      medium_status_ |= kMediumStatusBeaconing;

      std::optional<std::string> computer_name = DeviceInfo().GetOsDeviceName();
      if (computer_name.has_value()) {
        std::string device_name = absl::AsciiStrToUpper(computer_name.value());
        LOG(INFO) << "GO Device Name(Computer Name) is:" << device_name;
        credentials_go_ = wifi_direct_credentials;
        // Current pairing scheme uses ConfirmOnly, so pin is empty;
        credentials_go_->SetPin("");
        credentials_go_->SetDeviceName(device_name);
        return true;
      }
      LOG(ERROR) << "Windows WIFI Direct AutoGO failed to get computer name";
    }
    LOG(ERROR) << "Windows WIFI Direct AutoGO fails to start";
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Cannot start WifiDirect GO. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Cannot start WifiDirect GO.  WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }

  if (listener_) {
    listener_.ConnectionRequested(connection_requested_token_);
  }
  if (publisher_) {
    publisher_.StatusChanged(publisher_status_changed_token_);
  }
  listener_ = nullptr;
  publisher_ = nullptr;
  return false;
}

bool WifiDirectMedium::StopWifiDirect() {
  std::vector<std::unique_ptr<WifiDirectDeviceDiscovered>> devices;
  {
    absl::MutexLock lock(mutex_);
    devices.reserve(connection_requested_devices_by_id_.size());
    for (auto& [id, device] : connection_requested_devices_by_id_) {
      devices.push_back(std::move(device));
    }
    connection_requested_devices_by_id_.clear();
  }

  for (auto& device : devices) {
    LOG(INFO) << "Unpair WifiDirect GC: " << device->GetId();
    DeviceInformationPairing pairing = device->GetDeviceInformation().Pairing();
    if (pairing.IsPaired()) {
      LOG(INFO) << "GC Paired, unpair it";
      DeviceUnpairingResult unpairing_result = pairing.UnpairAsync().get();
      LOG(INFO) << "GC Unpair result:"
                << static_cast<int>(unpairing_result.Status());
      if (unpairing_result.Status() == DeviceUnpairingResultStatus::Unpaired) {
        LOG(INFO) << "GC Unpaired successfully";
      } else {
        LOG(INFO) << "GC Unpair failed";
      }
    } else {
      LOG(INFO) << "GC Not Paired, skip";
    }
  }

  absl::MutexLock lock(mutex_);
  is_ip_address_ready_.SignalAll();

  if (!IsBeaconing()) {
    LOG(WARNING)
        << "Cannot stop advertising because no advertising is running.";
    return true;
  }
  try {
    if (publisher_) {
      publisher_.Stop();
      listener_.ConnectionRequested(connection_requested_token_);
      publisher_.StatusChanged(publisher_status_changed_token_);
      wifi_direct_device_ = nullptr;
      listener_ = nullptr;
      publisher_ = nullptr;
      LOG(INFO) << "succeeded to stop WIFI advertising";
    }
    medium_status_ &= (~kMediumStatusBeaconing);
    medium_status_ &= (~kMediumStatusConnected);
    medium_status_ &= (~kMediumStatusAccepting);
    server_socket_ptr_ = nullptr;
    ip_address_local_.clear();
    ip_address_remote_.clear();
    return true;
  } catch (const std::exception& exception) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GO failed. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GO failed. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

fire_and_forget WifiDirectMedium::OnStatusChanged(
    WiFiDirectAdvertisementPublisher sender,
    WiFiDirectAdvertisementPublisherStatusChangedEventArgs event) {
  LOG(INFO) << "WIFI direct PublisherStatusChangedEvent: "
            << static_cast<int>(event.Status());
  if (event.Status() == WiFiDirectAdvertisementPublisherStatus::Started) {
    LOG(INFO) << "Receive WiFi direct/SoftAP Started event.";
    if (sender.Advertisement().LegacySettings().IsEnabled()) {
      LOG(INFO) << "WIFI direct Legacy AP ssid: "
                << winrt::to_string(
                       publisher_.Advertisement().LegacySettings().Ssid());
      LOG(INFO) << "WIFI direct Legacy AP pw: "
                << winrt::to_string(publisher_.Advertisement()
                                        .LegacySettings()
                                        .Passphrase()
                                        .Password());
    }
    return winrt::fire_and_forget();
  } else if (event.Status() ==
             WiFiDirectAdvertisementPublisherStatus::Created) {
    LOG(INFO) << "Receive WiFi direct/SoftAP Created event.";
    return winrt::fire_and_forget();
  } else if (event.Status() ==
             WiFiDirectAdvertisementPublisherStatus::Stopped) {
    LOG(INFO) << "Receive WiFi direct/SoftAP Stopped event.";
  } else if (event.Status() ==
             WiFiDirectAdvertisementPublisherStatus::Aborted) {
    LOG(INFO) << "Receive WiFi direct/SoftAP Aborted event.";
  }
  // Publisher is stopped. Need to clean up the publisher.
  {
    absl::MutexLock lock(mutex_);
    if (publisher_ != nullptr) {
      LOG(ERROR) << "Windows WiFi Direct cleanup.";
      listener_.ConnectionRequested(connection_requested_token_);
      publisher_.StatusChanged(publisher_status_changed_token_);
      wifi_direct_device_ = nullptr;
      listener_ = nullptr;
      publisher_ = nullptr;
      medium_status_ &= (~kMediumStatusBeaconing);
    }
  }
  return winrt::fire_and_forget();
}

fire_and_forget WifiDirectMedium::OnConnectionRequested(
    WiFiDirectConnectionListener const& sender,
    WiFiDirectConnectionRequestedEventArgs const& event) {
  WiFiDirectConnectionRequest connection_request = event.GetConnectionRequest();
  winrt::hstring device_name = connection_request.DeviceInformation().Name();
  winrt::hstring device_id = connection_request.DeviceInformation().Id();
  LOG(INFO) << "Receive connection request from: "
            << winrt::to_string(device_name)
            << "; device ID: " << winrt::to_string(device_id);

  DeviceInformation windows_device_info(connection_request.DeviceInformation());
  auto deviceInfoP =
      std::make_unique<WifiDirectDeviceDiscovered>(windows_device_info);

  {
    absl::MutexLock lock(&mutex_);
    connection_requested_devices_by_id_[device_id] = std::move(deviceInfoP);
  }

  bool is_paired = false;
  DeviceInformationPairing pairing =
      connection_request.DeviceInformation().Pairing();
  WiFiDirectConfigurationMethod config_method =
      WiFiDirectConfigurationMethod::PushButton;

  if (pairing.IsPaired() || IsAepPaired(device_id)) {
    if (pairing.IsPaired()) {
      LOG(INFO) << "GO Paired";
    } else {
      LOG(INFO) << "GO Not Paired, but AEP is paired";
    }
    LOG(INFO) << "GO already paired, unpair it first";
    DeviceUnpairingResult unpairing_result = pairing.UnpairAsync().get();
    LOG(INFO) << "GO Unpair result:"
              << static_cast<int>(unpairing_result.Status());
    if (unpairing_result.Status() == DeviceUnpairingResultStatus::Unpaired ||
        unpairing_result.Status() ==
            DeviceUnpairingResultStatus::AlreadyUnpaired) {
      LOG(INFO) << "GO Unpaired GC, Re-pair";
      // Wait for kWaitingForRePair to allow WiFi driver to stabilize.
      absl::SleepFor(kWaitingForRePair);
      // Refresh device info after unpairing.
      DeviceInformation refreshed_device_info =
          DeviceInformation::CreateFromIdAsync(device_id).get();
      is_paired = RequestPairDeviceAsync(refreshed_device_info.Pairing(), 14,
                                         config_method);
    } else {
      is_paired = true;
      LOG(INFO) << "GO Unpair failed, skip pairing";
    }
  } else {
    LOG(INFO) << "GO trying to pair with GC";
    is_paired = RequestPairDeviceAsync(pairing, 14, config_method);
  }

  if (is_paired) {
    WiFiDirectDevice device = nullptr;
    try {
      device = WiFiDirectDevice::FromIdAsync(device_id).get();
    } catch (winrt::hresult_error const& ex) {
      LOG(ERROR) << __func__ << ": winrt exception: " << ex.code() << ": "
                 << winrt::to_string(ex.message());
      return winrt::fire_and_forget();
    }

    device.ConnectionStatusChanged(
        {this, &WifiDirectMedium::OnConnectionStatusChanged});

    IVectorView<EndpointPair> endpoint_pairs =
        device.GetConnectionEndpointPairs();

    if (endpoint_pairs.Size() > 0) {
      auto const& pair = endpoint_pairs.GetAt(0);
      std::string local_ip =
          winrt::to_string(pair.LocalHostName().DisplayName());
      std::string remote_ip =
          winrt::to_string(pair.RemoteHostName().DisplayName());

      absl::MutexLock lock(&mutex_);
      wifi_direct_device_ = device;
      ip_address_local_ = local_ip;
      ip_address_remote_ = remote_ip;
      LOG(INFO) << "GO: Local IP: " << ip_address_local_
                << ", Remote IP: " << ip_address_remote_;
      is_ip_address_ready_.SignalAll();
    } else {
      LOG(WARNING) << "GO: No connection endpoint pairs found.";
    }
  }
  return winrt::fire_and_forget();
}

// In Windows, a single physical device can appear in the system as multiple
// different "objects" (e.g., a WiFi Direct object, a Bluetooth object, etc.).
// When a WiFi Direct connection request comes in, the code checks if Windows
// already has a "Paired" record for that physical MAC address under a different
// category. If it finds one, it considers the device "already known" to the
// system. The goal is to find and remove any stale pairing records that might
// cause the new WiFi Direct pairing to fail or hang.
bool WifiDirectMedium::IsAepPaired(winrt::hstring device_id) {
  try {
    DeviceInformation device_info =
        DeviceInformation::CreateFromIdAsync(
            device_id, {L"System.Devices.Aep.DeviceAddress"})
            .get();

    auto properties = device_info.Properties();
    if (!properties.HasKey(L"System.Devices.Aep.DeviceAddress")) {
      return false;
    }

    winrt::hstring aep_device_address = winrt::unbox_value<winrt::hstring>(
        properties.Lookup(L"System.Devices.Aep.DeviceAddress"));
    LOG(INFO) << "aep_device_address: " << winrt::to_string(aep_device_address);
    if (aep_device_address.empty()) {
      return false;
    }

    winrt::hstring device_selector =
        L"System.Devices.Aep.DeviceAddress:=\"" + aep_device_address + L"\"";
    LOG(INFO) << "Finding devices with selector: "
              << winrt::to_string(device_selector);
    DeviceInformationCollection device_collection =
        DeviceInformation::FindAllAsync(device_selector,
                                        {L"System.Devices.Aep.IsPaired"},
                                        DeviceInformationKind::Device)
            .get();

    LOG(INFO) << "Found " << device_collection.Size()
              << " devices with that MAC address.";
    for (auto const& device : device_collection) {
      LOG(INFO) << "Checking device: " << winrt::to_string(device.Name())
                << ", Id: " << winrt::to_string(device.Id());
      auto pairing = device.Pairing();
      if (pairing && pairing.IsPaired()) {
        LOG(INFO) << "Device is paired.";
        return true;
      }
      LOG(INFO) << "Device is not paired.";
    }
    return false;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << " failed. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << " failed. WinRT exception: " << error.code()
               << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

// Returns true once the WifiLan discovery has been initiated.
bool WifiDirectMedium::ConnectWifiDirect(
    const WifiDirectCredentials& credentials) {
  DisconnectWifiDirect();
  LOG(INFO) << "WifiDirectMedium::ConnectWifiDirect";
  {
    absl::MutexLock lock(mutex_);
    if (IsConnecting()) {
      LOG(WARNING) << "GC discovery already running.";
      return false;
    }

    if (IsBeaconing()) {
      LOG(WARNING) << "Already acting as GO, skip discovery.";
      return false;
    }

    if (device_watcher_) {
      LOG(WARNING)
          << "Device Watcher has already been set, please investigate! Skip";
      return false;
    }

    credentials_gc_ = credentials;
    if (credentials_gc_.GetDeviceName().empty()) {
      LOG(ERROR) << "GC: Device name is empty, return false";
      return false;
    }

    try {
      discovered_devices_by_id_.clear();
      connection_requested_devices_by_id_.clear();
      winrt::hstring device_selector = WiFiDirectDevice::GetDeviceSelector(
          WiFiDirectDeviceSelectorType::AssociationEndpoint);
      const winrt::param::iterable<winrt::hstring> requested_properties =
          winrt::single_threaded_vector<winrt::hstring>(
              {winrt::to_hstring(
                   "System.Devices.WiFiDirect.InformationElements"),
               winrt::to_hstring("System.Devices.Aep.CanPair"),
               winrt::to_hstring("System.Devices.Aep.IsPaired")});
      device_watcher_ = DeviceInformation::CreateWatcher(
          device_selector, requested_properties,
          DeviceInformationKind::AssociationEndpoint);
      device_watcher_added_event_token_ =
          device_watcher_.Added({this, &WifiDirectMedium::Watcher_DeviceAdded});
      device_watcher_updated_event_token_ = device_watcher_.Updated(
          {this, &WifiDirectMedium::Watcher_DeviceUpdated});
      device_watcher_removed_event_token_ = device_watcher_.Removed(
          {this, &WifiDirectMedium::Watcher_DeviceRemoved});
      device_watcher_enumeration_completed_event_token_ =
          device_watcher_.EnumerationCompleted(
              {this, &WifiDirectMedium::Watcher_DeviceEnumerationCompleted});
      device_watcher_stopped_event_token_ = device_watcher_.Stopped(
          {this, &WifiDirectMedium::Watcher_DeviceStopped});
      connection_latch_ = std::make_unique<CountDownLatch>(1);
      device_watcher_.Start();
      medium_status_ |= kMediumStatusConnecting;
    } catch (const std::exception& exception) {
      LOG(ERROR) << __func__ << " failed. Exception: " << exception.what();
      goto error;
    } catch (const winrt::hresult_error& error) {
      LOG(ERROR) << __func__ << " failed. WinRT exception: " << error.code()
                 << ": " << winrt::to_string(error.message());
      goto error;
    } catch (...) {
      LOG(ERROR) << __func__ << ": Unknown exception.";
      goto error;
    }
  }

  LOG(INFO) << "Started to discover and wait " << kConnectTimeout
            << " for connection.";
  connection_latch_->Await(kConnectTimeout);
  {
    absl::MutexLock lock(mutex_);
    if (IsConnected()) {
      LOG(INFO) << "WifiDirectMedium::ConnectWifiDirect succeeded.";
      return true;
    } else {
      LOG(WARNING) << "WifiDirectMedium::ConnectWifiDirect failed.";
    }
  }

error:
  {
    absl::MutexLock lock(mutex_);
    LOG(ERROR) << "GC discovery failed or pairing to GO failed.";
    if (device_watcher_) {
      device_watcher_.Stop();
      device_watcher_.Added(device_watcher_added_event_token_);
      device_watcher_.Updated(device_watcher_updated_event_token_);
      device_watcher_.Removed(device_watcher_removed_event_token_);
      device_watcher_.EnumerationCompleted(
          device_watcher_enumeration_completed_event_token_);
      device_watcher_.Stopped(device_watcher_stopped_event_token_);
    }

    device_watcher_ = nullptr;
    medium_status_ &= (~kMediumStatusConnecting);
    medium_status_ &= (~kMediumStatusConnected);
  }
  return false;
}

bool WifiDirectMedium::DisconnectWifiDirect() {
  LOG(INFO) << "WifiDirectMedium::DisconnectWifiDirect";
  std::vector<std::unique_ptr<WifiDirectDeviceDiscovered>> devices;
  {
    absl::MutexLock lock(mutex_);
    devices.reserve(discovered_devices_by_id_.size());
    for (auto& [id, device] : discovered_devices_by_id_) {
      devices.push_back(std::move(device));
    }
    discovered_devices_by_id_.clear();
  }

  for (auto& device : devices) {
    LOG(INFO) << "Unpair WifiDirect GO: " << device->GetId();
    DeviceInformationPairing pairing = device->GetDeviceInformation().Pairing();
    if (pairing.IsPaired()) {
      LOG(INFO) << "GC Paired, unpair it";
      DeviceUnpairingResult unpairing_result = pairing.UnpairAsync().get();
      LOG(INFO) << "GC Unpair result:"
                << static_cast<int>(unpairing_result.Status());
      if (unpairing_result.Status() == DeviceUnpairingResultStatus::Unpaired) {
        LOG(INFO) << "GC Unpaired successfully";
      } else {
        LOG(INFO) << "GC Unpair failed";
      }
    } else {
      LOG(INFO) << "GC Not Paired, skip";
    }
  }

  absl::MutexLock lock(mutex_);
  if (!IsConnecting() && !IsConnected()) {
    LOG(WARNING) << "WifiDirect GC is not connecting, skip";
    return true;
  }
  LOG(WARNING) << "Stop connecting.";
  try {
    if (device_watcher_) {
      device_watcher_.Stop();
      device_watcher_.Added(device_watcher_added_event_token_);
      device_watcher_.Updated(device_watcher_updated_event_token_);
      device_watcher_.EnumerationCompleted(
          device_watcher_enumeration_completed_event_token_);
      device_watcher_.Removed(device_watcher_removed_event_token_);
      device_watcher_.Stopped(device_watcher_stopped_event_token_);
      device_watcher_ = nullptr;
      ip_address_local_.clear();
      ip_address_remote_.clear();
    }
    medium_status_ &= (~kMediumStatusConnecting);
    medium_status_ &= (~kMediumStatusConnected);
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GC failed. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GC failed. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

fire_and_forget WifiDirectMedium::Watcher_DeviceAdded(
    DeviceWatcher sender, DeviceInformation device_info) {
  LOG(INFO) << "Device found for device ID "
            << winrt::to_string(device_info.Id())
            << ";   device name: " << winrt::to_string(device_info.Name());
  winrt::hstring device_id = device_info.Id();
  {
    absl::MutexLock lock(&mutex_);
    if (discovered_devices_by_id_.contains(device_id)) {
      return winrt::fire_and_forget();
    }
    std::string device_name_to_match = credentials_gc_.GetDeviceName();
    if (!absl::EqualsIgnoreCase(device_name_to_match,
                                winrt::to_string(device_info.Name()))) {
      LOG(INFO) << "We are looking for device: " << device_name_to_match
                << ", but found: " << winrt::to_string(device_info.Name())
                << ", skip.";
      return winrt::fire_and_forget();
    }
    discovered_devices_by_id_[device_id] =
        std::make_unique<WifiDirectDeviceDiscovered>(device_info);
  }
  LOG(INFO) << "Connect to device name: "
            << winrt::to_string(device_info.Name());
  DeviceInformationPairing pairing = device_info.Pairing();
  // WiFiDirectConfigurationMethod config_method =
  //     WiFiDirectConfigurationMethod::ProvidePin;
  WiFiDirectConfigurationMethod config_method =
      WiFiDirectConfigurationMethod::PushButton;
  bool is_paired;
  if (pairing.IsPaired()) {
    LOG(INFO) << "GC Paired, unpair it first to clean up stale state";
    DeviceUnpairingResult unpairing_result = pairing.UnpairAsync().get();
    LOG(INFO) << "GC Unpair result: "
              << static_cast<int>(unpairing_result.Status());
    if (unpairing_result.Status() == DeviceUnpairingResultStatus::Unpaired ||
        unpairing_result.Status() ==
            DeviceUnpairingResultStatus::AlreadyUnpaired) {
      // Wait kWaitingForRePair for the device stabilize before re-pairing.
      // This may avoid the possible contention problems in Intel WiFi driver.
      absl::SleepFor(kWaitingForRePair);
      DeviceInformation refreshed_device_info =
          DeviceInformation::CreateFromIdAsync(device_id).get();
      is_paired = RequestPairDeviceAsync(refreshed_device_info.Pairing(), 1,
                                         config_method);
      LOG(INFO) << "GC Re-Paired after unpair: " << is_paired;
    } else {
      LOG(INFO) << "GC Unpair failed, assume it's still paired.";
      is_paired =
          true;  // Fallback to true if unpair fails, maybe it's still usable.
    }
  } else {
    LOG(INFO) << "GC Not Paired, start to pair";
    is_paired = RequestPairDeviceAsync(device_info.Pairing(), 1, config_method);
  }
  // Create a WiFiDirectDevice out of this id
  if (!is_paired) {
    LOG(INFO) << "GC paired failed!";
    absl::MutexLock lock(&mutex_);
    if (connection_latch_) {
      connection_latch_->CountDown();
    }
    return fire_and_forget();
  }
  WiFiDirectDevice::FromIdAsync(device_info.Id())
      .Completed(
          [this, device_info](
              IAsyncOperation<WiFiDirectDevice> wifidirectDevice,
              AsyncStatus status) {
            absl::MutexLock lock(mutex_);
            WiFiDirectDevice(wifidirectDevice.get())
                .ConnectionStatusChanged(
                    {this, &WifiDirectMedium::OnConnectionStatusChanged});
            IVectorView<EndpointPair> endpoint_pairs =
                WiFiDirectDevice(wifidirectDevice.get())
                    .GetConnectionEndpointPairs();
            if (endpoint_pairs.Size() > 0) {
              auto const& pair = endpoint_pairs.GetAt(0);
              ip_address_local_ =
                  winrt::to_string(pair.LocalHostName().DisplayName());
              ip_address_remote_ =
                  winrt::to_string(pair.RemoteHostName().DisplayName());
              LOG(INFO) << "GC: Local IP: " << ip_address_local_
                        << ", Remote IP: " << ip_address_remote_;
              medium_status_ |= kMediumStatusConnected;
              if (connection_latch_) {
                connection_latch_->CountDown();
              }
            } else {
              LOG(WARNING) << "GC: No connection endpoint pairs found.";
            }
          });
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::Watcher_DeviceUpdated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  LOG(INFO) << "device updated for device ID "
            << winrt::to_string(deviceInfoUpdate.Id());
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::Watcher_DeviceRemoved(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  LOG(INFO) << "device removed for device ID "
            << winrt::to_string(deviceInfoUpdate.Id());
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::Watcher_DeviceEnumerationCompleted(
    DeviceWatcher sender, IInspectable inspectable) {
  LOG(INFO) << "DeviceWatcher enumeration completed!";
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::Watcher_DeviceStopped(
    DeviceWatcher sender, IInspectable inspectable) {
  LOG(INFO) << "DeviceWatcher stopped!";
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::OnPairingRequested(
    DeviceInformationCustomPairing const& sender,
    DevicePairingRequestedEventArgs const& event) {
  LOG(INFO) << "Handle Pairing Kind";
  switch (event.PairingKind()) {
    case DevicePairingKinds::DisplayPin:
      LOG(INFO) << "Display pin is: " << winrt::to_string(event.Pin());
      event.Accept();
      break;
    case DevicePairingKinds::ConfirmOnly:
      LOG(INFO) << "DevicePairingKinds::ConfirmOnly";
      event.Accept();
      break;
    case DevicePairingKinds::ProvidePin: {
      absl::MutexLock lock(mutex_);
      std::string pin;
      LOG(INFO) << "Enter pin:";
      std::cin >> pin;
      LOG(INFO) << "DevicePairingKinds::ProvidePin:" << pin;
      event.Accept(winrt::to_hstring(pin));
    } break;
    default:
      LOG(INFO) << "DevicePairingKinds::"
                << static_cast<int>(event.PairingKind());
      break;
  }
  return winrt::fire_and_forget();
}
void WifiDirectMedium::OnConnectionStatusChanged(
    WiFiDirectDevice const& sender,
    winrt::Windows::Foundation::IInspectable const&) {
  LOG(INFO) << "Connection status: "
            << static_cast<int>(sender.ConnectionStatus());
}
bool WifiDirectMedium::RequestPairDeviceAsync(
    DeviceInformationPairing pairing, int group_owner_intent,
    WiFiDirectConfigurationMethod config_method) {
  LOG(INFO) << __func__ << " Group Intent: " << group_owner_intent;
  WiFiDirectConnectionParameters connectionParams;
  connectionParams.GroupOwnerIntent(group_owner_intent);
  connectionParams.PreferenceOrderedConfigurationMethods().Append(
      config_method);
  DevicePairingKinds devicePairingKinds =
      WiFiDirectConnectionParameters::GetDevicePairingKinds(config_method);
  LOG(INFO) << "DevicePairingKinds: " << static_cast<int>(devicePairingKinds);
  connectionParams.PreferredPairingProcedure(
      WiFiDirectPairingProcedure::Invitation);
  DeviceInformationCustomPairing customPairing = pairing.Custom();
  customPairing.PairingRequested({this, &WifiDirectMedium::OnPairingRequested});
  DevicePairingResult result =
      customPairing
          .PairAsync(devicePairingKinds, DevicePairingProtectionLevel::Default,
                     connectionParams)
          .get();
  if (result.Status() != DevicePairingResultStatus::Paired &&
      result.Status() != DevicePairingResultStatus::AlreadyPaired) {
    LOG(INFO) << "Pair result: " << static_cast<int>(result.Status());
    return false;
  }
  LOG(INFO) << "Pair success ";
  return true;
}

std::vector<WifiDirectMedium::WifiDirectAuthType>
WifiDirectMedium::GetSupportedWifiDirectAuthTypes() const {
  // Windows only supports WifiDirect with Device Name Discovery.
  return {WifiDirectAuthType::WIFI_DIRECT_WITH_DEVICE_NAME};
}

}  // namespace nearby::windows
