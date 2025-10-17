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
#include "absl/time/time.h"
#include "internal/base/masker.h"
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
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Credentials.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/implementation/windows/wifi_intel.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {
namespace {
using ::absl::Milliseconds;
using ::winrt::Windows::Devices::WiFiDirect::
    WiFiDirectAdvertisementPublisherStatus;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectConnectionRequest;
using ::winrt::Windows::Security::Credentials::PasswordCredential;
}  // namespace

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

  bool dual_stack = NearbyFlags::GetInstance().GetBoolFlag(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableIpv6DualStack);
  SocketAddress server_address(dual_stack);
  if (!server_address.FromString(server_address, std::string(ip_address),
                                 port)) {
    LOG(ERROR) << "no valid service address and port to connect.";
    return nullptr;
  }
  VLOG(1) << "ConnectToService address: " << server_address.ToString();

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

  VLOG(1) << "maximum connection retries="
          << wifi_hotspot_max_connection_retries
          << ", connection interval=" << wifi_hotspot_retry_interval_millis
          << "ms, connection timeout="
          << wifi_hotspot_client_socket_connect_timeout_millis << "ms";

  LOG(INFO) << "Connect to service.";
  for (int i = 0; i < wifi_hotspot_max_connection_retries; ++i) {
    auto wifi_hotspot_socket = std::make_unique<WifiHotspotSocket>();

    // setup cancel listener
    std::unique_ptr<CancellationFlagListener> connection_cancellation_listener =
        nullptr;
    if (cancellation_flag != nullptr) {
      if (cancellation_flag->Cancelled()) {
        LOG(INFO) << "connect to service has been cancelled.";
        return nullptr;
      }

      connection_cancellation_listener =
          std::make_unique<nearby::CancellationFlagListener>(
              cancellation_flag, [socket = wifi_hotspot_socket.get()]() {
                LOG(WARNING) << "connect is closed due to it is cancelled.";
                socket->Close();
              });
    }

    bool result = wifi_hotspot_socket->Connect(server_address);
    if (!result) {
      LOG(WARNING) << "reconnect to service at " << (i + 1) << "th times";
      Sleep(wifi_hotspot_retry_interval_millis);
      continue;
    }

    LOG(INFO) << "connected to remote service.";
    return wifi_hotspot_socket;
  }

  LOG(ERROR) << "Failed to connect to service.";
  return nullptr;
}

std::unique_ptr<api::WifiHotspotServerSocket>
WifiHotspotMedium::ListenForService(int port) {
  absl::MutexLock lock(mutex_);
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

  bool dual_stack = NearbyFlags::GetInstance().GetBoolFlag(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableIpv6DualStack);
  if (server_socket->Listen(dual_stack)) {
    medium_status_ |= kMediumStatusAccepting;

    // Setup close notifier after listen started.
    server_socket->SetCloseNotifier([this]() {
      absl::MutexLock lock(mutex_);
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
    HotspotCredentials* hotspot_credentials) {
  absl::MutexLock lock(mutex_);
  VLOG(1) << __func__ << ": Start to create WiFi Hotspot.";

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

    // Enable Autonomous GO mode
    publisher_.Advertisement().IsAutonomousGroupOwnerEnabled(true);

    // Using WIFIDirect legacy mode to create a softAP. AP means "access point".
    Prng prng;
    publisher_.Advertisement().LegacySettings().IsEnabled(true);
    std::string password = absl::StrFormat("%08x", prng.NextUint32());
    hotspot_credentials->SetPassword(password);
    PasswordCredential creds;
    creds.Password(winrt::to_hstring(password));
    publisher_.Advertisement().LegacySettings().Passphrase(creds);

    std::string ssid = "DIRECT-" + std::to_string(prng.NextUint32());
    hotspot_credentials->SetSSID(ssid);
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
          hotspot_credentials->SetFrequency(
              WifiUtils::ConvertChannelToFrequencyMhz(GO_channel,
                                                      WifiBandType::kUnknown));
        }
      } else {
        LOG(INFO) << "Intel PIE disabled, Can't extract Hotspot channel info!";
        hotspot_credentials->SetFrequency(-1);
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
  absl::MutexLock lock(mutex_);

  if (!IsBeaconing()) {
    LOG(WARNING) << "Cannot stop SoftAP because no SoftAP is started.";
    return true;
  }
  try {
    if (publisher_) {
      publisher_.Stop();
      listener_.ConnectionRequested(connection_requested_token_);
      publisher_.StatusChanged(publisher_status_changed_token_);
      wifi_direct_devices_.clear();
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
      VLOG(1) << "WiFi SoftAP password: "
              << masker::Mask(winrt::to_string(publisher_.Advertisement()
                                                   .LegacySettings()
                                                   .Passphrase()
                                                   .Password()));
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
      LOG(ERROR) << "Windows WiFi Hotspot cleanup.";
      listener_.ConnectionRequested(connection_requested_token_);
      publisher_.StatusChanged(publisher_status_changed_token_);
      wifi_direct_devices_.clear();
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
    // We found new connection request comes in during hotspot transfer. In this
    // case, we should create a new WiFiDirectDevice for it. It will cause
    // transfer failure if replace the old WiFiDirectDevice with it.
    auto wifi_direct_device = WiFiDirectDevice::FromIdAsync(
                                  connection_request.DeviceInformation().Id())
                                  .get();
    wifi_direct_devices_.push_back(wifi_direct_device);
    LOG(INFO) << "Registered the device " << winrt::to_string(device_name)
              << " in WLAN-AutoConfig";
  } catch (...) {
    LOG(ERROR) << "Failed to registered the device "
               << winrt::to_string(device_name) << " in WLAN-AutoConfig";
    connection_request.Close();
  }
  return winrt::fire_and_forget();
}

bool WifiHotspotMedium::ConnectWifiHotspot(
    HotspotCredentials* hotspot_credentials) {
  absl::MutexLock lock(mutex_);

  try {
    if (IsConnected()) {
      LOG(WARNING) << "Already connected to Hotspot, disconnect first.";
      wifi_hotspot_native_.DisconnectWifiNetwork();
    }

    // Initialize Intel PIE scan if it is installed.
    bool intel_wifi_started = false;
    if (NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableIntelPieSdk)) {
      auto channel = WifiUtils::ConvertFrequencyMhzToChannel(
          hotspot_credentials->GetFrequency());
      WifiIntel& intel_wifi{WifiIntel::GetInstance()};
      intel_wifi_started = intel_wifi.Start();
      if (intel_wifi_started) {
        intel_wifi.SetScanFilter(channel);
      }
    }

    if (NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableWifiHotspotNativeScan)) {
      if (!wifi_hotspot_native_.Scan(hotspot_credentials->GetSSID())) {
        LOG(INFO) << "Hotspot " << hotspot_credentials->GetSSID()
                  << " is not found";

        if (intel_wifi_started) {
          WifiIntel& intel_wifi{WifiIntel::GetInstance()};
          intel_wifi.ResetScanFilter();
          intel_wifi.Stop();
        }
        return false;
      }
    }

    bool connected =
        wifi_hotspot_native_.ConnectToWifiNetwork(hotspot_credentials);

    if (intel_wifi_started) {
      WifiIntel& intel_wifi{WifiIntel::GetInstance()};
      intel_wifi.ResetScanFilter();
      intel_wifi.Stop();
    }

    if (!connected) {
      LOG(INFO) << "Failed to connect to Hotspot.";
      wifi_hotspot_native_.RestoreWifiProfile();
      return false;
    }

    LOG(INFO) << "Connected to Hotspot successfully.";

    // Make sure IP address is ready.
    bool has_address = false;
    int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
        platform::config_package_nearby::nearby_platform_feature::
            kWifiHotspotCheckIpMaxRetries);
    int64_t ip_address_retry_interval_millis =
        NearbyFlags::GetInstance().GetInt64Flag(
            platform::config_package_nearby::nearby_platform_feature::
                kWifiHotspotCheckIpIntervalMillis);
    absl::Duration connection_timeout =
        Milliseconds(ip_address_retry_interval_millis) * ip_address_max_retries;
    VLOG(1) << "maximum IP check retries=" << ip_address_max_retries
            << ", IP check interval=" << ip_address_retry_interval_millis
            << "ms, timeout=" << connection_timeout;
    absl::Time start_time = SystemClock::ElapsedRealtime();;
    for (int i = 0; i < ip_address_max_retries; i++) {
      LOG(INFO) << "Check IP address at attempt " << i;

      if (wifi_hotspot_native_.HasAssignedAddress()) {
        has_address = true;
        break;
      }
      // Keep track of time spent waiting for connection as RenewIpv4Address()
      // can take a while and only relying on retry count can increase the time
      // spent waiting significantly.
      if (SystemClock::ElapsedRealtime() - start_time > connection_timeout) {
        LOG(WARNING) << "Timeout getting IP address from hotspot.";
        break;
      }
      bool wait_before_retrying = true;
      if (NearbyFlags::GetInstance().GetBoolFlag(
              platform::config_package_nearby::nearby_platform_feature::
                  kEnableHotspotDhcpRenew)) {
        // IP address is assigned if RenewIpv4Address() returns true.
        wait_before_retrying = !wifi_hotspot_native_.RenewIpv4Address();
      }
      if (wait_before_retrying) {
        Sleep(ip_address_retry_interval_millis);
      }
    }

    if (!has_address) {
      LOG(WARNING) << "Failed to get IP address from hotspot.";
      wifi_hotspot_native_.RestoreWifiProfile();
      return false;
    }
    medium_status_ |= kMediumStatusConnected;
    LOG(INFO) << "Connected to hotspot: " << hotspot_credentials->GetSSID();

    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Cannot connet to Hotspot. Exception: " << exception.what();
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

bool WifiHotspotMedium::DisconnectWifiHotspot() {
  absl::MutexLock lock(mutex_);

  if (!IsConnected()) {
    LOG(WARNING) << "Cannot disconnect SoftAP because it is not connected.";
    return true;
  }

  wifi_hotspot_native_.RestoreWifiProfile();
  medium_status_ &= (~kMediumStatusConnected);
  LOG(INFO) << __func__ << ": Disconnected to hotspot successfully.";
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

}  // namespace nearby::windows
