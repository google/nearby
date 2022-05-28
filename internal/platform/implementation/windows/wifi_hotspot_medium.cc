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

#include "internal/platform/implementation/windows/wifi_hotspot.h"

// Nearby connections headers
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/logging.h"
#include "internal/platform/implementation/windows/utils.h"

namespace location {
namespace nearby {
namespace windows {

namespace {
  constexpr int kMaxRetries = 3;
  constexpr int kRetryIntervalMilliSeconds = 300;
  constexpr int kMaxScans = 3;
}  // namespace

WifiHotspotMedium::WifiHotspotMedium() {
  HotspotCredentials hotspot_credentials_;
  hotspot_interface_valid_ = StartWifiHotspot(&hotspot_credentials_);
  if (hotspot_interface_valid_)
    StopWifiHotspot();
}

WifiHotspotMedium::~WifiHotspotMedium() {
  StopWifiHotspot();
  DisconnectWifiHotspot();
}

bool WifiHotspotMedium::IsInterfaceValid() const {
  return hotspot_interface_valid_;
}


std::unique_ptr<api::WifiHotspotSocket> WifiHotspotMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag* cancellation_flag) {

  if (ip_address.empty() || port == 0) {
    NEARBY_LOGS(ERROR) << "no valid service address and port to connect: "
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
    NEARBY_LOGS(ERROR) << "Invalid IP address parameter.";
    return nullptr;
  }

  HostName host_name{winrt::to_hstring(ipv4_address)};
  winrt::hstring service_name{winrt::to_hstring(port)};

  StreamSocket socket{};

  // setup cancel listener
  if (cancellation_flag != nullptr) {
    if (cancellation_flag->Cancelled()) {
      NEARBY_LOGS(INFO) << "connect has been cancelled to service "
                        << ipv4_address << ":" << port;
      return nullptr;
    }

    location::nearby::CancellationFlagListener cancellationFlagListener(
        cancellation_flag, [socket]() { socket.CancelIOAsync().get(); });
  }

  // Try connecting to the service up to kMaxRetries. Because it may fail fisrt
  // time if DHCP procedure is not finished yet.
  for (int i = 0; i < kMaxRetries; i++) {
    try {
      socket.ConnectAsync(host_name, service_name).get();

      auto wifi_hotspot_socket = std::make_unique<WifiHotspotSocket>(socket);

      NEARBY_LOGS(INFO) << "connected to remote service " << ipv4_address << ":"
                        << port;
      return wifi_hotspot_socket;
    } catch (...) {
      NEARBY_LOGS(ERROR) << "failed to connect remote service " << ipv4_address
                         << ":" << port << " for the " << i+1 << " time";
      Sleep(kRetryIntervalMilliSeconds);
    }
  }
  return nullptr;
}

std::unique_ptr<api::WifiHotspotServerSocket>
WifiHotspotMedium::ListenForService(int port) {
  absl::MutexLock lock(&mutex_);

  // check current status
  if (IsAccepting()) {
    NEARBY_LOGS(WARNING) << "accepting connections already started on port "
                         << server_socket_ptr_->GetPort();
    return nullptr;
  }

  auto server_socket = std::make_unique<WifiHotspotServerSocket>(port);
  server_socket_ptr_ = server_socket.get();

  server_socket->SetCloseNotifier([this]() {
    absl::MutexLock lock(&mutex_);
    NEARBY_LOGS(INFO) << "server socket was closed on port "
                      << server_socket_ptr_->GetPort();
    medium_status_ &= (~kMediumStatusAccepting);
    server_socket_ptr_ = nullptr;
  });

  if (server_socket->listen()) {
    medium_status_ |= kMediumStatusAccepting;
    NEARBY_LOGS(INFO) << "started to listen serive on port "
                      << server_socket_ptr_->GetPort();
    return server_socket;
  }

  NEARBY_LOGS(ERROR) << "Failed to listen service on port " << port;

  return nullptr;
}

bool WifiHotspotMedium::StartWifiHotspot(
    HotspotCredentials* hotspot_credentials_) {
  absl::MutexLock lock(&mutex_);

  if (IsBeaconing()) {
    NEARBY_LOGS(WARNING) << "cannot create SoftAP again when it is running.";
    return true;
  }

  publisher_ = WiFiDirectAdvertisementPublisher();
  publisher_status_changed_token_ =
      publisher_.StatusChanged({this, &WifiHotspotMedium::OnStatusChanged});
  listener_ = WiFiDirectConnectionListener();
  try {
    listener_.ConnectionRequested(
        {this, &WifiHotspotMedium::OnConnectionRequested});
  } catch (winrt::hresult_error const& ex) {
    NEARBY_LOGS(ERROR) << __func__ << ": winrt exception: " << ex.code() << ": "
                       << winrt::to_string(ex.message());
  }

  // Normal mode: The device is highly discoverable so long as the app is in the
  // foreground.
  publisher_.Advertisement().ListenStateDiscoverability(
      WiFiDirectAdvertisementListenStateDiscoverability::Normal);
  // Enbale Autonomous GO mode
  publisher_.Advertisement().IsAutonomousGroupOwnerEnabled(true);

  // Using WIFIDirect legacy mode to create a softAP. AP means "access point".
  publisher_.Advertisement().LegacySettings().IsEnabled(true);
  std::string password = absl::StrFormat("%08x", Prng().NextUint32());
  hotspot_credentials_->SetPassword(password);
  PasswordCredential creds;
  creds.Password(winrt::to_hstring(password));
  publisher_.Advertisement().LegacySettings().Passphrase(creds);

  std::string ssid = "DIRECT-" + std::to_string(Prng().NextUint32());
  hotspot_credentials_->SetSSID(ssid);
  publisher_.Advertisement().LegacySettings().Ssid(winrt::to_hstring(ssid));

  publisher_.Start();
  if (publisher_.Status() == WiFiDirectAdvertisementPublisherStatus::Started) {
    NEARBY_LOGS(INFO) << "Windows WiFi Hotspot started";
    medium_status_ |= kMediumStatusBeaconing;

    return true;
  }

  // Clean up when fail
  NEARBY_LOGS(ERROR) << "Windows WiFi Hotspot fails to start";
  listener_ = nullptr;
  publisher_ = nullptr;
  return false;
}

bool WifiHotspotMedium::StopWifiHotspot() {
  // Need to use Win32 API to deregister the Dnssd instance
  absl::MutexLock lock(&mutex_);

  if (!IsBeaconing()) {
    NEARBY_LOGS(WARNING) << "Cannot stop SoftAP because no SoftAP is started.";
    return true;
  }

  if (publisher_) {
    publisher_.Stop();
    publisher_.StatusChanged(publisher_status_changed_token_);
    listener_.ConnectionRequested(connection_requested_token_);
    NEARBY_LOGS(INFO) << "succeeded to stop WiFi Hotspot";
  }
  medium_status_ &= (~kMediumStatusBeaconing);
  return true;
}

fire_and_forget WifiHotspotMedium::OnStatusChanged(
    WiFiDirectAdvertisementPublisher sender,
    WiFiDirectAdvertisementPublisherStatusChangedEventArgs event) {
  if (event.Status() == WiFiDirectAdvertisementPublisherStatus::Started) {
    if (sender.Advertisement().LegacySettings().IsEnabled()) {
      NEARBY_LOGS(INFO)
          << "WiFi SoftAP SSID: "
          << winrt::to_string(
                 publisher_.Advertisement().LegacySettings().Ssid());
      NEARBY_LOGS(INFO) << "WiFi SoftAP PW: "
                        << winrt::to_string(publisher_.Advertisement()
                                                .LegacySettings()
                                                .Passphrase()
                                                .Password());
    }
  }
  if (event.Status() == WiFiDirectAdvertisementPublisherStatus::Stopped) {
    NEARBY_LOGS(INFO) << "Receive WiFi direct/SoftAP stop event";
  }
  return winrt::fire_and_forget();
}

fire_and_forget WifiHotspotMedium::OnConnectionRequested(
    WiFiDirectConnectionListener const& sender,
    WiFiDirectConnectionRequestedEventArgs const& event) {
  WiFiDirectConnectionRequest connection_request = event.GetConnectionRequest();
  winrt::hstring device_name = connection_request.DeviceInformation().Name();
  NEARBY_LOGS(INFO) << "Receive connection request from: "
                    << winrt::to_string(device_name);

  DeviceInformationPairing pairing =
      connection_request.DeviceInformation().Pairing();
  if (pairing.IsPaired())
    NEARBY_LOGS(INFO) << "Paired";
  else
    NEARBY_LOGS(INFO) << "Not Paired";

  return winrt::fire_and_forget();
}

bool WifiHotspotMedium::ConnectWifiHotspot(
    HotspotCredentials* hotspot_credentials_) {
  absl::MutexLock lock(&mutex_);

  if (IsConnected()) {
    NEARBY_LOGS(WARNING) << "Already connected to AP, disconnect first.";
    DisconnectWifiHotspot();
  }

  auto access = WiFiAdapter::RequestAccessAsync().get();
  if (access != WiFiAccessStatus::Allowed) {
    NEARBY_LOGS(WARNING) << "Access Denied with reason: "
                         << static_cast<int>(access);
    return false;
  }

  auto adaptors = WiFiAdapter::FindAllAdaptersAsync().get();
  if (adaptors.Size() < 1) {
    NEARBY_LOGS(WARNING) << "No WiFi Adaptor found.";
    return false;
  }
  wifi_adapter_ = adaptors.GetAt(0);

  // SoftAP is an abbreviation for "software enabled access point".
  WiFiAvailableNetwork nearby_softap{nullptr};
  NEARBY_LOGS(INFO) << "Scanning for Nearby Hotspot SSID: "
                    << hotspot_credentials_->GetSSID();

  // First time scan may not find our target hotspot, try 3 times can almost
  // guarantee to find the Hotspot
  for (int i = 0; i < kMaxScans; i++) {
    for (const auto& network :
         wifi_adapter_.NetworkReport().AvailableNetworks()) {
      if (winrt::to_string(network.Ssid()) == hotspot_credentials_->GetSSID()) {
        NEARBY_LOGS(INFO) << "Found Nearby SSID: "
                          << winrt::to_string(network.Ssid());
        nearby_softap = network;
        break;
      }
    }
    if (nearby_softap) break;
    NEARBY_LOGS(INFO) << "Scan ... ";
    wifi_adapter_.ScanAsync().get();
  }

  if (!nearby_softap) {
    NEARBY_LOGS(INFO) << "Hotspot is not found";
    return false;
  }

  PasswordCredential creds;
  creds.Password(winrt::to_hstring(hotspot_credentials_->GetPassword()));

  auto connect_result =
      wifi_adapter_
          .ConnectAsync(nearby_softap, WiFiReconnectionKind::Automatic, creds)
          .get();

  if (connect_result.ConnectionStatus() != WiFiConnectionStatus::Success) {
    NEARBY_LOGS(INFO) << "Connectiong failed with reason: "
                      << static_cast<int>(connect_result.ConnectionStatus());
    return false;
  }

  NEARBY_LOGS(INFO) << "Connected to: " << hotspot_credentials_->GetSSID();
  medium_status_ |= kMediumStatusConnected;
  return true;
}

bool WifiHotspotMedium::DisconnectWifiHotspot() {
  absl::MutexLock lock(&mutex_);

  if (!IsConnected()) {
    NEARBY_LOGS(WARNING)
        << "Cannot diconnect SoftAP because it is not connected.";
  }
  if (wifi_adapter_) {
    wifi_adapter_.Disconnect();
    NEARBY_LOGS(INFO) << "Disconnect softAP.";
    wifi_adapter_ = {nullptr};
  }
  medium_status_ &= (~kMediumStatusConnected);
  return true;
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
}  // namespace location
