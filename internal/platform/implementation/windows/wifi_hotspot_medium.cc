// Copyright 2021 Google LLC
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
#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/output_file.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/implementation/windows/wifi_intel.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace windows {
namespace {
constexpr absl::string_view kHotspotSsidFileName = "ssid.txt";
}

WifiHotspotMedium::WifiHotspotMedium() {
  std::string ssid = GetStoredHotspotSsid();
  if (!ssid.empty()) {
    LOG(INFO) << "Get stored Hotspot SSID: " << ssid
                      << " from previous run";
    DeleteNetworkProfile(winrt::to_hstring(ssid));
    StoreHotspotSsid({});
  }
}

WifiHotspotMedium::~WifiHotspotMedium() {
  StopWifiHotspot();
  DisconnectWifiHotspot();
}

bool WifiHotspotMedium::IsInterfaceValid() const {
  HANDLE wifi_direct_handle = nullptr;
  DWORD negotiated_version = 0;
  DWORD result = 0;

  result =
      WFDOpenHandle(WFD_API_VERSION, &negotiated_version, &wifi_direct_handle);
  if (result == ERROR_SUCCESS) {
    LOG(INFO) << "WiFi can support Hotspot";
    WFDCloseHandle(wifi_direct_handle);
    return true;
  }

  LOG(ERROR) << "WiFi can't support Hotspot";
  return false;
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag* cancellation_flag) {
  LOG(WARNING) << __func__ << " : Connect to remote service.";

  if (ip_address.empty() || port == 0) {
    LOG(ERROR) << "no valid service address and port to connect: "
               << "ip_address = " << ip_address << ", port = " << port;
    return nullptr;
  }

  std::string ipv4_address;
  if (ip_address.length() == 4) {
    ipv4_address = ipaddr_4bytes_to_dotdecimal_string(ip_address);
  } else {
    ipv4_address = std::string(ip_address);
  }
  if (ipv4_address.empty()) {
    LOG(ERROR) << "Invalid IP address parameter.";
    return nullptr;
  }

  HostName host_name{winrt::to_hstring(ipv4_address)};
  winrt::hstring service_name{winrt::to_hstring(port)};

  // Try connecting to the service up to wifi_hotspot_max_connection_retries,
  // because it may fail first time if DHCP procedure is not finished yet.
  int64_t wifi_hotspot_max_connection_retries =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionMaxRetries);
  int64_t wifi_hotspot_retry_interval_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionIntervalMillis);
  int64_t wifi_hotspot_client_socket_connect_timeout_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotConnectionTimeoutMillis);

  LOG(INFO) << "maximum connection retries="
            << wifi_hotspot_max_connection_retries
            << ", connection interval=" << wifi_hotspot_retry_interval_millis
            << "ms, connection timeout="
            << wifi_hotspot_client_socket_connect_timeout_millis << "ms";
  for (int i = 0; i < wifi_hotspot_max_connection_retries; i++) {
    try {
      StreamSocket socket{};
      // Listener to connect cancellation.
      std::unique_ptr<CancellationFlagListener>
          connection_cancellation_listener = nullptr;

      // setup cancel listener
      if (cancellation_flag != nullptr) {
        if (cancellation_flag->Cancelled()) {
          LOG(INFO) << "connect has been cancelled to service " << ipv4_address
                    << ":" << port;
          return nullptr;
        }

        connection_cancellation_listener =
            std::make_unique<nearby::CancellationFlagListener>(
                cancellation_flag, [socket]() {
                  LOG(WARNING) << "connect is closed due to it is cancelled.";
                  socket.Close();
                });
      }

      if (FeatureFlags::GetInstance().GetFlags().enable_connection_timeout) {
        connection_timeout_ = scheduled_executor_.Schedule(
            [socket]() {
              LOG(WARNING) << "connect is closed due to timeout.";
              socket.Close();
            },
            absl::Milliseconds(
                wifi_hotspot_client_socket_connect_timeout_millis));
      }

      socket.ConnectAsync(host_name, service_name).get();

      if (connection_timeout_ != nullptr) {
        connection_timeout_->Cancel();
        connection_timeout_ = nullptr;
      }

      auto wifi_hotspot_socket = std::make_unique<WifiHotspotSocket>(socket);

      LOG(INFO) << "connected to remote service " << ipv4_address << ":"
                << port;
      return wifi_hotspot_socket;
    } catch (std::exception exception) {
      LOG(ERROR) << "failed to connect remote service " << ipv4_address << ":"
                 << port << " for the " << i + 1
                 << " time. Exception: " << exception.what();
    } catch (const winrt::hresult_error& error) {
      LOG(ERROR) << "failed to connect remote service " << ipv4_address << ":"
                 << port << " for the " << i + 1
                 << " time. WinRT exception: " << error.code() << ": "
                 << winrt::to_string(error.message());
    } catch (...) {
      LOG(ERROR) << "failed to connect remote service " << ipv4_address << ":"
                 << port << " for the " << i + 1
                 << " time due to unknown reason.";
    }

    if (connection_timeout_ != nullptr) {
      connection_timeout_->Cancel();
      connection_timeout_ = nullptr;
    }

    Sleep(wifi_hotspot_retry_interval_millis);
  }
  return nullptr;
}

std::unique_ptr<api::WifiHotspotServerSocket>
WifiHotspotMedium::ListenForService(int port) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__
            << " :Start to listen connection from WiFi Hotspot client.";

  // check current status
  if (IsAccepting()) {
    LOG(WARNING) << "accepting connections already started on port "
                 << server_socket_ptr_->GetPort();
    return nullptr;
  }

  auto server_socket = std::make_unique<WifiHotspotServerSocket>(port);
  server_socket_ptr_ = server_socket.get();

  if (server_socket->listen()) {
    medium_status_ |= kMediumStatusAccepting;

    // Setup close notifier after listen started.
    server_socket->SetCloseNotifier([this]() {
      absl::MutexLock lock(&mutex_);
      LOG(INFO) << "Server socket was closed.";
      medium_status_ &= (~kMediumStatusAccepting);
      server_socket_ptr_ = nullptr;
    });
    LOG(INFO) << "Started to listen serive on port "
              << server_socket_ptr_->GetPort();
    return server_socket;
  }

  LOG(ERROR) << "Failed to listen service on port " << port;

  return nullptr;
}

bool WifiHotspotMedium::StartWifiHotspot(
    HotspotCredentials* hotspot_credentials_) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": Start to create WiFi Hotspot.";

  if (IsBeaconing()) {
    LOG(WARNING) << "Cannot create WiFi Hotspot again when it is running.";
    return true;
  }

  try {
    publisher_ = WiFiDirectAdvertisementPublisher();
    publisher_status_changed_token_ =
        publisher_.StatusChanged({this, &WifiHotspotMedium::OnStatusChanged});
    listener_ = WiFiDirectConnectionListener();
    connection_requested_token_ = listener_.ConnectionRequested(
        {this, &WifiHotspotMedium::OnConnectionRequested});

    // Normal mode: The device is highly discoverable so long as the app is in
    // the foreground.
    publisher_.Advertisement().ListenStateDiscoverability(
        WiFiDirectAdvertisementListenStateDiscoverability::Normal);
    // Enbale Autonomous GO mode
    publisher_.Advertisement().IsAutonomousGroupOwnerEnabled(true);

    // Using WIFIDirect legacy mode to create a softAP. AP means "access point".
    Prng prng;
    publisher_.Advertisement().LegacySettings().IsEnabled(true);
    std::string password = absl::StrFormat("%08x", prng.NextUint32());
    hotspot_credentials_->SetPassword(password);
    PasswordCredential creds;
    creds.Password(winrt::to_hstring(password));
    publisher_.Advertisement().LegacySettings().Passphrase(creds);

    std::string ssid = "DIRECT-" + std::to_string(prng.NextUint32());
    hotspot_credentials_->SetSSID(ssid);
    publisher_.Advertisement().LegacySettings().Ssid(winrt::to_hstring(ssid));

    publisher_.Start();
    if (publisher_.Status() ==
        WiFiDirectAdvertisementPublisherStatus::Started) {
      LOG(INFO) << __func__ << ": WiFi Hotspot created and started.";
      medium_status_ |= kMediumStatusBeaconing;
      if (NearbyFlags::GetInstance().GetBoolFlag(
              platform::config_package_nearby::nearby_platform_feature::
                  kEnableIntelPieSdk)) {
        WifiIntel& intel_wifi{WifiIntel::GetInstance()};
        if (intel_wifi.Start()) {
          int GO_channel = intel_wifi.GetGOChannel();
          LOG(INFO) << "Intel PIE enabled, Hotspot is running on channel: "
                    << GO_channel;
          intel_wifi.Stop();
          hotspot_credentials_->SetFrequency(
              WifiUtils::ConvertChannelToFrequencyMhz(GO_channel,
                                                      WifiBandType::kUnknown));
        }
      } else {
        LOG(INFO) << "Intel PIE disabled, Can't extract Hotspot channel info!";
        hotspot_credentials_->SetFrequency(-1);
      }
      return true;
    }

    // Clean up when fail
    LOG(ERROR) << "Windows WiFi Hotspot fails to start";
    publisher_.StatusChanged(publisher_status_changed_token_);
    listener_.ConnectionRequested(connection_requested_token_);
    listener_ = nullptr;
    publisher_ = nullptr;
    return false;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Cannot start Hotspot. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Cannot start Hotspot.  WinRT exception: " << error.code()
               << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
  return false;
}

bool WifiHotspotMedium::StopWifiHotspot() {
  // Need to use Win32 API to deregister the Dnssd instance
  absl::MutexLock lock(&mutex_);

  if (!IsBeaconing()) {
    LOG(WARNING) << "Cannot stop SoftAP because no SoftAP is started.";
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
      LOG(INFO) << "succeeded to stop WiFi Hotspot";
    }

    medium_status_ &= (~kMediumStatusBeaconing);
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Stop Hotspot failed. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Stop Hotspot failed. WinRT exception: " << error.code()
               << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
  return false;
}

fire_and_forget WifiHotspotMedium::OnStatusChanged(
    WiFiDirectAdvertisementPublisher sender,
    WiFiDirectAdvertisementPublisherStatusChangedEventArgs event) {
  if (event.Status() == WiFiDirectAdvertisementPublisherStatus::Started) {
    if (sender.Advertisement().LegacySettings().IsEnabled()) {
      LOG(INFO) << "WiFi SoftAP SSID: "
                << winrt::to_string(
                       publisher_.Advertisement().LegacySettings().Ssid());
      LOG(INFO) << "WiFi SoftAP PW: "
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
    absl::MutexLock lock(&mutex_);
    if (publisher_ != nullptr) {
      LOG(ERROR) << "Windows WiFi Hotspot cleanup.";
      listener_.ConnectionRequested(connection_requested_token_);
      publisher_.StatusChanged(publisher_status_changed_token_);
      wifi_direct_device_ = nullptr;
      listener_ = nullptr;
      publisher_ = nullptr;
    }
  }

  return winrt::fire_and_forget();
}

fire_and_forget WifiHotspotMedium::OnConnectionRequested(
    WiFiDirectConnectionListener const& sender,
    WiFiDirectConnectionRequestedEventArgs const& event) {
  WiFiDirectConnectionRequest connection_request = event.GetConnectionRequest();
  winrt::hstring device_name = connection_request.DeviceInformation().Name();
  LOG(INFO) << "Receive connection request from: "
            << winrt::to_string(device_name);

  try {
    // This is to solve b/236805122.
    // Problem: [Microsoft-Windows-WLAN-AutoConfig] issues a disconnection to
    // Hotspot Client every 2 minutes and stopped Hotspot eventually.
    // Solution: Creating a WiFiDirectDevice for Client’s connection request can
    // solve the problem. Guess when this object is created,
    // [Microsoft-Windows-WLAN-AutoConfig] will recognise it as a valid device
    // and won't kick it away.
    wifi_direct_device_ = WiFiDirectDevice::FromIdAsync(
                              connection_request.DeviceInformation().Id())
                              .get();
    LOG(INFO) << "Registered the device in WLAN-AutoConfig";
  } catch (...) {
    LOG(ERROR) << "Failed to registered the device in WLAN-AutoConfig";
    wifi_direct_device_ = nullptr;
    connection_request.Close();
  }
  return winrt::fire_and_forget();
}

bool WifiHotspotMedium::ConnectWifiHotspot(
    HotspotCredentials* hotspot_credentials_) {
  absl::MutexLock lock(&mutex_);

  try {
    if (!wifi_connected_hotspot_ssid_.empty()) {
      LOG(INFO) << "Before connecting to Hotspot, Delete the previous "
                   "Hotspot profile with SSID: "
                << winrt::to_string(wifi_connected_hotspot_ssid_);
      DeleteNetworkProfile(wifi_connected_hotspot_ssid_);
      wifi_connected_hotspot_ssid_ = winrt::hstring(L"");
      StoreHotspotSsid({});
    }
    if (IsConnected()) {
      LOG(WARNING) << "Already connected to Hotspot, disconnect first.";
      InternalDisconnectWifiHotspot();
    }

    auto access = WiFiAdapter::RequestAccessAsync().get();
    if (access != WiFiAccessStatus::Allowed) {
      LOG(WARNING) << "Access Denied with reason: " << static_cast<int>(access);
      return false;
    }

    auto adapters = WiFiAdapter::FindAllAdaptersAsync().get();
    if (adapters.Size() < 1) {
      LOG(WARNING) << "No WiFi Adapter found.";
      return false;
    }
    wifi_adapter_ = adapters.GetAt(0);

    // Retrieve the current connected network's profile
    ConnectionProfile profile =
        wifi_adapter_.NetworkAdapter().GetConnectedProfileAsync().get();
    std::string ssid;

    if (profile != nullptr && profile.IsWlanConnectionProfile()) {
      ssid = winrt::to_string(
          profile.WlanConnectionProfileDetails().GetConnectedSsid());
    }

    // SoftAP is an abbreviation for "software enabled access point".
    WiFiAvailableNetwork nearby_softap{nullptr};

    bool intel_wifi_started = false;
    if (NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableIntelPieSdk)) {
      auto channel = WifiUtils::ConvertFrequencyMhzToChannel(
          hotspot_credentials_->GetFrequency());
      WifiIntel& intel_wifi{WifiIntel::GetInstance()};
      intel_wifi_started = intel_wifi.Start();
      if (intel_wifi_started) {
        intel_wifi.SetScanFilter(channel);
      }
    }

    LOG(INFO) << "Scanning for Nearby Hotspot SSID: "
              << hotspot_credentials_->GetSSID();
    // First time scan may not find our target hotspot, try 2 more times can
    // almost guarantee to find the Hotspot
    wifi_adapter_.ScanAsync().get();

    wifi_original_network_ = nullptr;
    int64_t wifi_hotspot_max_scans = NearbyFlags::GetInstance().GetInt64Flag(
        platform::config_package_nearby::nearby_platform_feature::
            kWifiHotspotScanMaxRetries);

    int i;
    for (i = 0; i < wifi_hotspot_max_scans; i++) {
      for (const auto& network :
           wifi_adapter_.NetworkReport().AvailableNetworks()) {
        if (!wifi_original_network_ && !ssid.empty() &&
            (winrt::to_string(network.Ssid()) == ssid)) {
          wifi_original_network_ = network;
          LOG(INFO) << "Save the current connected network: " << ssid;
        } else if (!nearby_softap && winrt::to_string(network.Ssid()) ==
                                         hotspot_credentials_->GetSSID()) {
          LOG(INFO) << "Found Nearby SSID: "
                    << winrt::to_string(network.Ssid());
          nearby_softap = network;
        }
        if (nearby_softap && (ssid.empty() || wifi_original_network_)) break;
      }
      if (nearby_softap) break;
      LOG(INFO) << "Scan ... ";
      wifi_adapter_.ScanAsync().get();
    }
    LOG(INFO) << "Finish scanning "
              << (nearby_softap ? "successfully" : "failed") << " with "
              << i + 1 << " times trying.";

    if (intel_wifi_started) {
      WifiIntel& intel_wifi{WifiIntel::GetInstance()};
      intel_wifi.ResetScanFilter();
      intel_wifi.Stop();
    }

    if (!nearby_softap) {
      LOG(INFO) << "Hotspot is not found";
      return false;
    }
    PasswordCredential creds;
    creds.Password(winrt::to_hstring(hotspot_credentials_->GetPassword()));

    auto connect_result =
        wifi_adapter_
            .ConnectAsync(nearby_softap, WiFiReconnectionKind::Manual, creds)
            .get();

    if (connect_result == nullptr ||
        connect_result.ConnectionStatus() != WiFiConnectionStatus::Success) {
      LOG(INFO) << "Connecting failed with reason: "
                << static_cast<int>(connect_result.ConnectionStatus());
      RestoreWifiConnection();
      return false;
    }

    // Make sure IP address is ready.
    std::string ip_address;
    int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
        platform::config_package_nearby::nearby_platform_feature::
            kWifiHotspotCheckIpMaxRetries);
    int64_t ip_address_retry_interval_millis =
        NearbyFlags::GetInstance().GetInt64Flag(
            platform::config_package_nearby::nearby_platform_feature::
                kWifiHotspotCheckIpIntervalMillis);
    LOG(INFO) << "maximum IP check retries=" << ip_address_max_retries
              << ", IP check interval=" << ip_address_retry_interval_millis
              << "ms";
    for (int i = 0; i < ip_address_max_retries; i++) {
      LOG(INFO) << "Check IP address at attemp " << i;
      std::vector<std::string> ip_addresses = GetIpv4Addresses();
      if (ip_addresses.empty()) {
        Sleep(ip_address_retry_interval_millis);
        continue;
      }
      ip_address = ip_addresses[0];
      break;
    }

    if (ip_address.empty()) {
      LOG(INFO) << "Failed to get IP address from hotspot.";
      RestoreWifiConnection();
      DeleteNetworkProfile(nearby_softap.Ssid());
      return false;
    }

    LOG(INFO) << "Got IP address " << ip_address << " from hotspot.";

    std::string last_ssid = hotspot_credentials_->GetSSID();
    wifi_connected_hotspot_ssid_ = nearby_softap.Ssid();
    StoreHotspotSsid(winrt::to_string(wifi_connected_hotspot_ssid_));
    medium_status_ |= kMediumStatusConnected;
    LOG(INFO) << "Connected to hotspot: " << last_ssid;

    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Cannot connet to Hotspot. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Cannot connet to Hotspot.  WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
  return false;
}

void WifiHotspotMedium::RestoreWifiConnection() {
  if (!wifi_original_network_ && wifi_adapter_) {
    wifi_adapter_.Disconnect();
    return;
  }
  if (wifi_adapter_) {
    ConnectionProfile profile =
        wifi_adapter_.NetworkAdapter().GetConnectedProfileAsync().get();
    std::string ssid;

    if (profile != nullptr && profile.IsWlanConnectionProfile()) {
      ssid = winrt::to_string(
          profile.WlanConnectionProfileDetails().GetConnectedSsid());
      if (!ssid.empty() &&
          (winrt::to_string(wifi_original_network_.Ssid()) == ssid)) {
        LOG(INFO) << "Already conneted to the previous WIFI network " << ssid
                  << "! Skip restoration.";
        return;
      }
    }

    // Disconnect to the WiFi connection through the WiFi adapter.
    wifi_adapter_.Disconnect();
    LOG(INFO) << "Disconnected to current network.";

    auto connect_result = wifi_adapter_
                              .ConnectAsync(wifi_original_network_,
                                            WiFiReconnectionKind::Automatic)
                              .get();

    if (connect_result == nullptr ||
        connect_result.ConnectionStatus() != WiFiConnectionStatus::Success) {
      LOG(INFO) << "Connecting to previous network failed with reason: "
                << static_cast<int>(connect_result.ConnectionStatus());
    } else {
      LOG(INFO) << "Restored the previous WIFI connection: "
                << winrt::to_string(wifi_original_network_.Ssid());
    }
    wifi_original_network_ = nullptr;
  }
}

bool WifiHotspotMedium::DisconnectWifiHotspot() {
  absl::MutexLock lock(&mutex_);
  try {
    return InternalDisconnectWifiHotspot();
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Stop Hotspot failed. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Stop Hotspot failed. WinRT exception: " << error.code()
               << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
  return false;
}

bool WifiHotspotMedium::InternalDisconnectWifiHotspot() {
  if (!IsConnected()) {
    LOG(WARNING) << "Cannot disconnect SoftAP because it is not connected.";
    return true;
  }

  if (wifi_adapter_) {
    // Disconnect to the WiFi connection through the WiFi adapter.
    RestoreWifiConnection();
    wifi_adapter_ = nullptr;

    if (!wifi_connected_hotspot_ssid_.empty()) {
      LOG(INFO) << "Delete the previous connected network profile with SSID: "
                << winrt::to_string(wifi_connected_hotspot_ssid_);
      DeleteNetworkProfile(wifi_connected_hotspot_ssid_);
      wifi_connected_hotspot_ssid_ = winrt::hstring(L"");
      StoreHotspotSsid({});
    }
  }

  medium_status_ &= (~kMediumStatusConnected);
  return true;
}

bool WifiHotspotMedium::DeleteNetworkProfile(winrt::hstring ssid) {
  bool result = false;
  ConnectionProfile profile{nullptr};
  auto connections = NetworkInformation::GetConnectionProfiles();
  auto ssid_string = winrt::to_string(ssid);
  if (ssid_string.empty()) {
    LOG(INFO) << "SSID is empty. No need to delete the network profile";
    return true;
  }

  LOG(INFO) << "Search profile with SSID: " << ssid_string;
  for (const auto& connection_profile : connections) {
    if (connection_profile.ProfileName() == ssid) {
      LOG(INFO) << "Found the network profile with SSID: " << ssid_string;
      profile = connection_profile;
      break;
    }
  }
  if (profile == nullptr) {
    LOG(INFO) << "No network profile found with SSID: " << ssid_string;
    return result;
  }

  if (profile != nullptr && profile.CanDelete() &&
      profile.IsWlanConnectionProfile()) {
    auto profile_delete_status = profile.TryDeleteAsync().get();
    switch (profile_delete_status) {
      case ConnectionProfileDeleteStatus::Success:
        LOG(INFO) << "WiFi profile with SSID:" << ssid_string << " is deleted.";
        result = true;
        break;
      case ConnectionProfileDeleteStatus::DeniedBySystem:
        LOG(ERROR) << "Failed to delete WiFi profile with SSID:" << ssid_string
                   << " due to denied by system.";
        break;
      case ConnectionProfileDeleteStatus::DeniedByUser:
        LOG(ERROR) << "Failed to delete WiFi profile with SSID:" << ssid_string
                   << " due to denied by user.";
        break;
      case ConnectionProfileDeleteStatus::UnknownError:
        LOG(ERROR) << "Failed to delete WiFi profile with SSID:" << ssid_string
                   << " due to unknonw error.";
        break;
      default:
        break;
    }
  }
  return result;
}

void WifiHotspotMedium::StoreHotspotSsid(std::string ssid) {
  std::unique_ptr<api::OutputFile> ssid_file;
  try {
    std::string file_name(kHotspotSsidFileName);
    std::string full_path =
        nearby::api::ImplementationPlatform::GetAppDataPath(file_name);
    ssid_file =
        nearby::api::ImplementationPlatform::CreateOutputFile(full_path);
    if (ssid_file == nullptr) {
      LOG(ERROR) << "Failed to create output file: " << file_name;
      return;
    }
    ByteArray data(ssid);
    ssid_file->Write(data);
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
                       << ": Failed to store Hotspot SSID. Exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
                       << ": Failed to store Hotspot SSID. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
  }
  if (ssid_file != nullptr) {
    ssid_file->Close();
  }
}

std::string WifiHotspotMedium::GetStoredHotspotSsid() {
  std::unique_ptr<api::InputFile> ssid_file;
  try {
    std::string file_name(kHotspotSsidFileName);
    std::string full_path =
        nearby::api::ImplementationPlatform::GetAppDataPath(file_name);
    std::unique_ptr<api::InputFile> ssid_file =
        nearby::api::ImplementationPlatform::CreateInputFile(full_path, 0);
    if (ssid_file == nullptr) {
      LOG(ERROR) << "Failed to create input file: " << file_name;
      return {};
    }
    auto total_size = ssid_file->GetTotalSize();
    if (total_size == 0) {
      LOG(INFO) << __func__ << ": No Hotspot ssid found.";
      ssid_file->Close();
      return {};
    }

    nearby::ExceptionOr<ByteArray> raw_ssid = ssid_file->Read(total_size);

    if (!raw_ssid.ok()) {
      LOG(ERROR) << __func__
                         << ": Failed to read Hotspot ssid. Exception: "
                         << raw_ssid.exception();
      return {};
    }
    return std::string(raw_ssid.GetResult().data());
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
                       << ": Failed to store Hotspot SSID. Exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
                       << ": Failed to store Hotspot SSID. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
  }
  if (ssid_file != nullptr) {
    ssid_file->Close();
  }

  return {};
}

std::string WifiHotspotMedium::GetErrorMessage(std::exception_ptr eptr) {
  try {
    if (eptr) {
      std::rethrow_exception(eptr);
    } else {
      return "";
    }
  } catch (const std::exception& e) {
    return e.what();
  }
}

}  // namespace windows
}  // namespace nearby
