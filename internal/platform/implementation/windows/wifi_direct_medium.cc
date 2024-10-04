// Copyright 2022 Google LLC
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

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/implementation/windows/wifi_direct.h"

// Nearby connections headers
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {

// The maximum scanning times for GC to find WifiDirect GO.
constexpr int kWifiDirectMaxScans = 2;

// The maximum connection times to remote WifiDirect GO server socket.
constexpr int kWifiDirectMaxConnectionRetries = 3;

// The interval between 2 connectin attempts.
constexpr absl::Duration kWifiDirectRetryIntervalMillis =
    absl::Milliseconds(300);

// The connection timeout to remote WifiDirect GO server socket.
constexpr absl::Duration kWifiDirectClientSocketConnectTimeoutMillis =
    absl::Milliseconds(10000);

// The maximum times to check IP address.
constexpr int kIpAddressMaxRetries = 3;

// The time interval to check IP address.
constexpr absl::Duration kIpAddressRetryIntervalMillis =
    absl::Milliseconds(2000);

}  // namespace

WifiDirectMedium::~WifiDirectMedium() {
  StopWifiDirect();
  DisconnectWifiDirect();
}

bool WifiDirectMedium::IsInterfaceValid() const {
  HANDLE wifi_direct_handle = nullptr;
  DWORD negotiated_version = 0;
  DWORD result =
      WFDOpenHandle(WFD_API_VERSION, &negotiated_version, &wifi_direct_handle);
  if (result == ERROR_SUCCESS) {
    LOG(INFO) << "WiFi can support WifiDirect";
    WFDCloseHandle(wifi_direct_handle);
    return true;
  }

  LOG(ERROR) << "WiFi can't support WifiDirect";
  return false;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectMedium::ConnectToService(
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

  if (!WifiUtils::ValidateIPV4(ipv4_address)) {
    LOG(ERROR) << "Invalid IP address parameter.";
    return nullptr;
  }

  HostName host_name{winrt::to_hstring(ipv4_address)};
  winrt::hstring service_name{winrt::to_hstring(port)};

  // Try connecting to the service up to kWifiDirectMaxConnectionRetries,
  // because it may fail first time if DHCP procedure is not finished yet.
  for (int i = 0; i < kWifiDirectMaxConnectionRetries; i++) {
    try {
      StreamSocket socket{};

      std::unique_ptr<nearby::CancellationFlagListener>
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

      connection_timeout_ = scheduled_executor_.Schedule(
          [socket]() {
            LOG(WARNING) << "connect is closed due to timeout.";
            socket.Close();
          },
          kWifiDirectClientSocketConnectTimeoutMillis);

      socket.ConnectAsync(host_name, service_name).get();

      if (connection_timeout_ != nullptr) {
        connection_timeout_->Cancel();
        connection_timeout_ = nullptr;
      }

      auto client_socket = std::make_unique<WifiDirectSocket>(socket);

      LOG(INFO) << "connected to remote service " << ipv4_address << ":"
                << port;
      return client_socket;
    } catch (...) {
      LOG(ERROR) << "failed to connect remote service " << ipv4_address << ":"
                 << port << " for the " << i + 1 << " time";
    }

    if (connection_timeout_ != nullptr) {
      connection_timeout_->Cancel();
      connection_timeout_ = nullptr;
    }

    Sleep(kWifiDirectRetryIntervalMillis / absl::Milliseconds(1));
  }
  return nullptr;
}

std::unique_ptr<api::WifiDirectServerSocket> WifiDirectMedium::ListenForService(
    int port) {
  absl::MutexLock lock(&mutex_);

  // check current status
  if (IsAccepting()) {
    LOG(WARNING) << "accepting connections already started on port "
                 << server_socket_ptr_->GetPort();
    return nullptr;
  }

  auto server_socket = std::make_unique<WifiDirectServerSocket>(port);
  server_socket_ptr_ = server_socket.get();

  if (server_socket->listen()) {
    medium_status_ |= kMediumStatusAccepting;
    server_socket->SetCloseNotifier([this]() {
      absl::MutexLock lock(&mutex_);
      LOG(INFO) << "server socket was closed on port "
                << server_socket_ptr_->GetPort();
      medium_status_ &= (~kMediumStatusAccepting);
      server_socket_ptr_ = nullptr;
    });

    LOG(INFO) << "started to listen serive on port "
              << server_socket_ptr_->GetPort();
    return server_socket;
  }

  LOG(ERROR) << "Failed to listen service on port " << port;

  return nullptr;
}

bool WifiDirectMedium::StartWifiDirect(
    WifiDirectCredentials* wifi_direct_credentials) {
  absl::MutexLock lock(&mutex_);

  if (IsBeaconing()) {
    LOG(WARNING) << "cannot create SoftAP again when it is running.";
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

    // Using WIFIDirect legacy mode to create a softAP. AP means "access point".
    // Windows WinRT WifiDirect class follows a full P2P protocol to establish a
    // P2P connection. However the current Phone Nearby Connection uses a
    // simplified protocol for P2P connection, which is just like a legacy
    // AP/STA SSID/password mode. To accommodate with this, Windows side give up
    // the formal P2P connection code suite, instead it uses Hotspot and legacy
    // STA to fake a P2P connection. By this way, when Phone act as Group
    // Client(GC), it can keep the connection with Router(AP) at the same time.
    // But if phone act as Group Owner(GO), Windows side still can't keep its AP
    // connection when connecting to phone as a GC because it is in normal STA
    // mode.
    Prng prng;
    publisher_.Advertisement().LegacySettings().IsEnabled(true);
    std::string password = absl::StrFormat("%08x", prng.NextUint32());
    wifi_direct_credentials->SetPassword(password);
    PasswordCredential creds;
    creds.Password(winrt::to_hstring(password));
    publisher_.Advertisement().LegacySettings().Passphrase(creds);

    std::string ssid = "DIRECT-" + std::to_string(prng.NextUint32());
    wifi_direct_credentials->SetSSID(ssid);
    publisher_.Advertisement().LegacySettings().Ssid(winrt::to_hstring(ssid));

    publisher_.Start();
    if (publisher_.Status() ==
        WiFiDirectAdvertisementPublisherStatus::Started) {
      LOG(INFO) << "Windows WiFiDirect GO(SoftAP) started";
      medium_status_ |= kMediumStatusBeaconing;
      return true;
    }

    // Clean up when fail
    LOG(ERROR) << "Windows WiFiDirect GO(SoftAP) fails to start";
    publisher_.StatusChanged(publisher_status_changed_token_);
    listener_.ConnectionRequested(connection_requested_token_);
    listener_ = nullptr;
    publisher_ = nullptr;
    return false;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Cannot start WiFiDirect GO. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Cannot start WiFiDirect GO.  WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

bool WifiDirectMedium::StopWifiDirect() {
  // Need to use Win32 API to deregister the Dnssd instance
  absl::MutexLock lock(&mutex_);

  if (!IsBeaconing()) {
    LOG(WARNING)
        << "Cannot stop WiFiDirect GO(SoftAP) because no GO was started.";
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
      LOG(INFO) << "succeeded to stop WiFiDirect GO(SoftAP)";
    }

    medium_status_ &= (~kMediumStatusBeaconing);
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Stop WiFiDirect GO failed. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Stop WiFiDirect GO failed. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

fire_and_forget WifiDirectMedium::OnStatusChanged(
    WiFiDirectAdvertisementPublisher sender,
    WiFiDirectAdvertisementPublisherStatusChangedEventArgs event) {
  if (event.Status() == WiFiDirectAdvertisementPublisherStatus::Started) {
    if (sender.Advertisement().LegacySettings().IsEnabled()) {
      LOG(INFO) << "WiFiDirect GO SSID: "
                << winrt::to_string(
                       publisher_.Advertisement().LegacySettings().Ssid());
      LOG(INFO) << "WiFiDirect GO PW: "
                << winrt::to_string(publisher_.Advertisement()
                                        .LegacySettings()
                                        .Passphrase()
                                        .Password());
    }
    return winrt::fire_and_forget();
  } else if (event.Status() ==
             WiFiDirectAdvertisementPublisherStatus::Created) {
    LOG(INFO) << "Receive WiFiDirect/SoftAP Created event.";
    return winrt::fire_and_forget();
  } else if (event.Status() ==
             WiFiDirectAdvertisementPublisherStatus::Stopped) {
    LOG(INFO) << "Receive WiFiDirect/SoftAP Stopped event.";
  } else if (event.Status() ==
             WiFiDirectAdvertisementPublisherStatus::Aborted) {
    LOG(INFO) << "Receive WiFiDirect/SoftAP Aborted event.";
  }

  // Publisher is stopped. Need to clean up the publisher.
  {
    absl::MutexLock lock(&mutex_);
    if (publisher_ != nullptr) {
      LOG(ERROR) << "Windows WiFiDirect GO(SoftAP) cleanup.";
      listener_.ConnectionRequested(connection_requested_token_);
      publisher_.StatusChanged(publisher_status_changed_token_);
      wifi_direct_device_ = nullptr;
      listener_ = nullptr;
      publisher_ = nullptr;
    }
  }

  return winrt::fire_and_forget();
}

fire_and_forget WifiDirectMedium::OnConnectionRequested(
    WiFiDirectConnectionListener const& sender,
    WiFiDirectConnectionRequestedEventArgs const& event) {
  WiFiDirectConnectionRequest connection_request = event.GetConnectionRequest();
  winrt::hstring device_name = connection_request.DeviceInformation().Name();
  LOG(INFO) << "Receive connection request from: "
            << winrt::to_string(device_name);

  try {
    // This is to solve b/236805122.
    // Problem: [Microsoft-Windows-WLAN-AutoConfig] issues a disconnection to
    // WifiDirect Client every 2 minutes and stopped WifiDirect GO eventually.
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

bool WifiDirectMedium::ConnectWifiDirect(
    WifiDirectCredentials* wifi_direct_credentials) {
  absl::MutexLock lock(&mutex_);

  try {
    if (IsConnected()) {
      LOG(WARNING) << "Already connected to AP, disconnect first.";
      InternalDisconnectWifiDirect();
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
    LOG(INFO) << "Scanning for Nearby WifiDirect GO's SSID: "
              << wifi_direct_credentials->GetSSID();

    // First time scan may not find our target GO, try 2 more times can
    // almost guarantee to find the GO
    wifi_adapter_.ScanAsync().get();

    wifi_connected_network_ = nullptr;
    for (int i = 0; i < kWifiDirectMaxScans; i++) {
      for (const auto& network :
           wifi_adapter_.NetworkReport().AvailableNetworks()) {
        if (!wifi_connected_network_ && !ssid.empty() &&
            (winrt::to_string(network.Ssid()) == ssid)) {
          wifi_connected_network_ = network;
          LOG(INFO) << "Save the current connected network: " << ssid;
        } else if (!nearby_softap && winrt::to_string(network.Ssid()) ==
                                         wifi_direct_credentials->GetSSID()) {
          LOG(INFO) << "Found Nearby SSID: "
                    << winrt::to_string(network.Ssid());
          nearby_softap = network;
        }
        if (nearby_softap && wifi_connected_network_) break;
      }
      if (nearby_softap) break;
      LOG(INFO) << "Scan ... ";
      wifi_adapter_.ScanAsync().get();
    }

    if (!nearby_softap) {
      LOG(INFO) << "WifiDirect GO is not found";
      return false;
    }

    PasswordCredential creds;
    creds.Password(winrt::to_hstring(wifi_direct_credentials->GetPassword()));

    auto connect_result =
        wifi_adapter_
            .ConnectAsync(nearby_softap, WiFiReconnectionKind::Manual, creds)
            .get();

    if (connect_result == nullptr ||
        connect_result.ConnectionStatus() != WiFiConnectionStatus::Success) {
      LOG(INFO) << "Connecting failed with reason: "
                << static_cast<int>(connect_result.ConnectionStatus());
      return false;
    }

    // Make sure IP address is ready.
    std::string ip_address;
    for (int i = 0; i < kIpAddressMaxRetries; i++) {
      LOG(INFO) << "Check IP address at attempt " << i;
      std::vector<std::string> ip_addresses = GetIpv4Addresses();
      if (ip_addresses.empty()) {
        Sleep(kIpAddressRetryIntervalMillis / absl::Milliseconds(1));
        continue;
      }
      ip_address = ip_addresses[0];
      break;
    }

    if (ip_address.empty()) {
      LOG(INFO) << "Failed to get IP address from WifiDirect GO.";
      return false;
    }

    LOG(INFO) << "Got IP: " << ip_address << " from WifiDirect GO.";

    std::string last_ssid = wifi_direct_credentials->GetSSID();
    medium_status_ |= kMediumStatusConnected;
    LOG(INFO) << "Connected to WifiDirect GO: " << last_ssid;

    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Cannot connet to WifiDirect GO. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Cannot connet to WifiDirect GO.  WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

void WifiDirectMedium::RestoreWifiConnection() {
  if (!wifi_connected_network_ && wifi_adapter_) {
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
          (winrt::to_string(wifi_connected_network_.Ssid()) == ssid)) {
        LOG(INFO) << "Already conneted to the previous WIFI network " << ssid
                  << "! Skip restoration.";
        return;
      }
    }

    // Disconnect to the WiFi connection through the WiFi adapter.
    wifi_adapter_.Disconnect();
    LOG(INFO) << "Disconnected to current network.";

    auto connect_result = wifi_adapter_
                              .ConnectAsync(wifi_connected_network_,
                                            WiFiReconnectionKind::Automatic)
                              .get();

    if (connect_result == nullptr ||
        connect_result.ConnectionStatus() != WiFiConnectionStatus::Success) {
      LOG(INFO) << "Connecting to previous network failed with reason: "
                << static_cast<int>(connect_result.ConnectionStatus());
    } else {
      LOG(INFO) << "Restored the previous WIFI connection: "
                << winrt::to_string(wifi_connected_network_.Ssid());
    }
    wifi_connected_network_ = nullptr;
  }
}

bool WifiDirectMedium::DisconnectWifiDirect() {
  absl::MutexLock lock(&mutex_);
  try {
    return InternalDisconnectWifiDirect();
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Disconnect WifiDirect GO failed. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Disconnect WifiDirect GO failed. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

bool WifiDirectMedium::InternalDisconnectWifiDirect() {
  if (!IsConnected()) {
    LOG(WARNING)
        << "Cannot disconnect WifiDirect GO because it is not connected.";
    return true;
  }

  if (wifi_adapter_) {
    // Gets connected WiFi profile.
    auto profile =
        wifi_adapter_.NetworkAdapter().GetConnectedProfileAsync().get();

    // Disconnect to the WiFi connection through the WiFi adapter.
    RestoreWifiConnection();
    wifi_adapter_ = nullptr;

    // Try to remove the WiFi profile
    if (profile != nullptr && profile.CanDelete() &&
        profile.IsWlanConnectionProfile()) {
      std::string ssid = winrt::to_string(
          profile.WlanConnectionProfileDetails().GetConnectedSsid());

      auto profile_delete_status = profile.TryDeleteAsync().get();
      switch (profile_delete_status) {
        case ConnectionProfileDeleteStatus::Success:
          LOG(INFO) << "WiFi profile with SSID:" << ssid << " is deleted.";
          break;
        case ConnectionProfileDeleteStatus::DeniedBySystem:
          LOG(ERROR) << "Failed to delete WiFi profile with SSID:" << ssid
                     << " due to denied by system.";
          break;
        case ConnectionProfileDeleteStatus::DeniedByUser:
          LOG(ERROR) << "Failed to delete WiFi profile with SSID:" << ssid
                     << " due to denied by user.";
          break;
        case ConnectionProfileDeleteStatus::UnknownError:
          LOG(ERROR) << "Failed to delete WiFi profile with SSID:" << ssid
                     << " due to unknonw error.";
          break;
        default:
          break;
      }
    }
  }

  medium_status_ &= (~kMediumStatusConnected);
  return true;
}

std::string WifiDirectMedium::GetErrorMessage(std::exception_ptr eptr) {
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
