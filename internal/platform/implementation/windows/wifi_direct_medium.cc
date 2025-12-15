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
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/logging.h"
#include "internal/platform/prng.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace windows {
namespace {
constexpr int kWaitingForConnectionTimeoutSeconds = 90;  // seconds
}  // namespace

WifiDirectMedium::WifiDirectMedium() {
  LOG(INFO) << "WifiDirectMedium::WifiDirectMedium";
  // Create a DispatcherQueue for this thread.
  controller_ = winrt::Windows::System::DispatcherQueueController::
      CreateOnDedicatedThread();
  dispatcher_queue_ = controller_.DispatcherQueue();
  if (!dispatcher_queue_) {
    LOG(WARNING) << "Failed to get DispatcherQueue for current thread. "
                    "ConnectAsync might fail if not called from UI thread.";
  }
}

WifiDirectMedium::~WifiDirectMedium() {
  listener_executor_.Shutdown();
  StopWifiDirect();
  DisconnectWifiDirect();
  if (controller_) {
    // Asynchronously shut down the dispatcher queue.
    winrt::Windows::Foundation::IAsyncAction shutdown_async =
        controller_.ShutdownQueueAsync();

    // Block and wait for the shutdown to complete. This ensures that any
    // in-progress event handlers on the dedicated thread are finished
    // before this object is fully destroyed.
    shutdown_async.get();
  }
}

bool WifiDirectMedium::IsInterfaceValid() const {
  HANDLE wifi_direct_handle = nullptr;
  DWORD negotiated_version = 0;
  DWORD result = 0;

  result =
      WFDOpenHandle(WFD_API_VERSION, &negotiated_version, &wifi_direct_handle);
  if (result == ERROR_SUCCESS) {
    LOG(INFO) << "WiFi can support WifiDirect";
    WFDCloseHandle(wifi_direct_handle);
    return true;
  }

  LOG(ERROR) << "WiFi can't support WifiDirect";
  return false;
}

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

  std::string remote_ip_address;
  if (ip_address.empty()) {
    remote_ip_address = ip_address_remote_;
  } else {
    remote_ip_address = std::string(ip_address);
  }
  // when this API is called, GC may not finish connecting to GO, so we need to
  // wait the connection is finished and IP address is ready.
  if (remote_ip_address.empty()) {
    LOG(INFO) << "Waiting for IP address to be ready.";
    absl::MutexLock lock(mutex_);
    is_ip_address_ready_.WaitWithTimeout(
        &mutex_, absl::Seconds(kWaitingForConnectionTimeoutSeconds));
    if (ip_address_remote_.empty()) {
      LOG(WARNING)
          << "IP address is still empty, probably GC connecting to GO failed.";
      return nullptr;
    }
    LOG(INFO) << "IP address is ready.";
    remote_ip_address = ip_address_remote_;
  }

  if (remote_ip_address.empty() || port == 0) {
    LOG(ERROR) << "no valid service address and port to connect: "
               << "ip_address = " << remote_ip_address << ", port = " << port;
    return nullptr;
  }

  SocketAddress server_address(/*dual_stack=*/true);
  if (!server_address.FromString(server_address, remote_ip_address, port)) {
    LOG(ERROR) << "no valid service address and port to connect.";
    return nullptr;
  }
  VLOG(1) << "ConnectToService server address: " << server_address.ToString();

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

  VLOG(1) << "maximum connection retries=" << wifi_direct_max_connection_retries
          << ", connection interval=" << wifi_direct_retry_interval_millis
          << "ms, connection timeout="
          << wifi_direct_client_socket_connect_timeout_millis << "ms";

  LOG(INFO) << "Connect to service ";
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
    if (!result) {
      LOG(WARNING) << "reconnect to service at " << (i + 1) << "th times";
      Sleep(wifi_direct_retry_interval_millis);
      continue;
    }

    LOG(INFO) << "connected to remote service ";
    return wifi_direct_socket;
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
  if (!IsGOStarted()) {
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
        is_ip_address_ready_.WaitWithTimeout(
            &mutex_, absl::Seconds(kWaitingForConnectionTimeoutSeconds));
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
    if (server_socket_ptr_ &&
        server_socket_ptr_->Listen(port)) {
      medium_status_ |= kMediumStatusAccepting;

      // Setup close notifier after listen started.
      server_socket_ptr_->SetCloseNotifier([this]() {
        absl::MutexLock lock(mutex_);
        LOG(INFO) << "Server socket was closed.";
        medium_status_ &= (~kMediumStatusAccepting);
        server_socket_ptr_ = nullptr;
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
  LOG(INFO) << "WifiDirectMedium::StartWifiDirect";

  absl::MutexLock lock(mutex_);
  if (IsGOStarted()) {
    LOG(WARNING) << "Already started WifiDirect GO, skip.";
    return true;
  }

  credentials_go_ = wifi_direct_credentials;
  Prng prng;
  std::string pin = absl::StrFormat("%04x", prng.NextUint32());
  credentials_go_->SetPin(pin);

  std::string service_name = "NC-" + std::to_string(prng.NextUint32());
  credentials_go_->SetServiceName(service_name);
  LOG(INFO) << "service_name:pin " << service_name << ":" << pin;

  // Create Advertiser object
  advertiser_ = WiFiDirectServiceAdvertiser(winrt::to_hstring(service_name));
  advertisement_status_changed_token_ = advertiser_.AdvertisementStatusChanged(
      {this, &WifiDirectMedium::OnAdvertisementStatusChanged});
  auto_accept_session_connected_token_ = advertiser_.AutoAcceptSessionConnected(
      {this, &WifiDirectMedium::OnAutoAcceptSessionConnected});
  session_requested_token_ = advertiser_.SessionRequested(
      {this, &WifiDirectMedium::OnSessionRequested});

  advertiser_.AutoAcceptSession(false);
  advertiser_.PreferGroupOwnerMode(true);
  advertiser_.ServiceStatus(WiFiDirectServiceStatus::Available);
  // Config Methods
  WiFiDirectServiceConfigurationMethod config_method;
  if (pin.empty()) {
    config_method = WiFiDirectServiceConfigurationMethod::Default;  // NOLINT
  } else {
    config_method = WiFiDirectServiceConfigurationMethod::PinDisplay;
  }
  advertiser_.PreferredConfigurationMethods().Clear();
  advertiser_.PreferredConfigurationMethods().Append(config_method);

  try {
    advertiser_.Start();
    LOG(INFO) << "Start WifiDirect GO Status: "
              << (int)advertiser_.AdvertisementStatus();
    if ((advertiser_.AdvertisementStatus() ==
         WiFiDirectServiceAdvertisementStatus::Created) ||
        (advertiser_.AdvertisementStatus() ==
         WiFiDirectServiceAdvertisementStatus::Started)) {
      medium_status_ |= kMediumStatusGOStarted;
      return true;
    }
    LOG(ERROR) << "Start WifiDirect GO failed.";
    return false;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GO failed. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GO failed. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
  advertiser_.AdvertisementStatusChanged(advertisement_status_changed_token_);
  advertiser_.AutoAcceptSessionConnected(auto_accept_session_connected_token_);
  advertiser_.SessionRequested(session_requested_token_);
  advertiser_.PreferredConfigurationMethods().Clear();
  advertiser_ = nullptr;
  return false;
}

bool WifiDirectMedium::StopWifiDirect() {
  LOG(INFO) << "WifiDirectMedium::StopWifiDirect";
  absl::MutexLock lock(mutex_);
  if (!IsGOStarted()) {
    LOG(WARNING) << "Cannot stop Service because no Service is started.";
    return true;
  }

  try {
    if (advertiser_) {
      advertiser_.Stop();
      advertiser_.AdvertisementStatusChanged(
          advertisement_status_changed_token_);
      advertiser_.AutoAcceptSessionConnected(
          auto_accept_session_connected_token_);
      advertiser_.SessionRequested(session_requested_token_);
      advertiser_ = nullptr;
      device_info_ = nullptr;
      session_ = nullptr;
    }
    medium_status_ &= (~kMediumStatusGOStarted);
    medium_status_ &= (~kMediumStatusConnected);
    server_socket_ptr_ = nullptr;
    ip_address_local_.clear();
    ip_address_remote_.clear();
    listener_executor_.Shutdown();
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GO failed. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GO failed. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
  return false;
}

std::string WifiDirectMedium::ConfigMethodToString(
    WiFiDirectServiceConfigurationMethod config_method) {
  switch (config_method) {
    case WiFiDirectServiceConfigurationMethod::Default:
      return "Default";
    case WiFiDirectServiceConfigurationMethod::PinDisplay:
      return "PinDisplay";
    case WiFiDirectServiceConfigurationMethod::PinEntry:
      return "PinEntry";
    default:
      return "Unknown";
  }
}

fire_and_forget WifiDirectMedium::OnAdvertisementStatusChanged(
    WiFiDirectServiceAdvertiser sender, IInspectable const& event) {
  LOG(INFO) << "WiFiDirectServiceAdvertiser status changed: "
            << (int)sender.AdvertisementStatus();
  auto status = sender.ServiceStatus();
  switch (status) {
    case WiFiDirectServiceStatus ::Available:
      LOG(INFO) << "WifiDirectAdvertiser service status changed: "
                   "status: Available";
      break;
    case WiFiDirectServiceStatus ::Busy:
      LOG(INFO) << "WifiDirectAdvertiser service status changed: "
                   "status: Busy";
      break;
    case WiFiDirectServiceStatus ::Custom:
      LOG(INFO) << "WifiDirectAdvertiser service status changed: "
                   "status: Custom";
      break;
    default:
      LOG(INFO) << "WifiDirectAdvertiser service status changed: "
                   "Code: "
                << (int)status;
      break;
  }
  return winrt::fire_and_forget();
}

fire_and_forget WifiDirectMedium::OnAutoAcceptSessionConnected(
    WiFiDirectServiceAdvertiser sender,
    WiFiDirectServiceAutoAcceptSessionConnectedEventArgs const& args) {
  LOG(INFO) << "WifiDirectMedium::OnAutoAcceptSessionConnected";
  try {
    auto session = args.Session();
    if (!session) {
      LOG(ERROR) << "OnAutoAcceptSessionConnected returned null session";
      co_return;
    }
    session_ = std::move(session);
    LOG(INFO) << "Service Address: "
              << winrt::to_string(session_.ServiceAddress())
              << ", Service Name: " << winrt::to_string(session_.ServiceName())
              << ", Advertisement ID: " << session_.AdvertisementId()
              << ", Session Address: "
              << winrt::to_string(session_.SessionAddress())
              << ", Session ID: " << session_.SessionId();
    // Subscribe to events to prevent early teardown
    session_.SessionStatusChanged([](auto const& s, auto const& e) {
      LOG(INFO) << "GO: Session status changed";
    });
    co_return;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Failed to get session. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Failed to get session. WinRT exception: " << error.code()
               << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
}

fire_and_forget WifiDirectMedium::OnSessionRequested(
    WiFiDirectServiceAdvertiser const& sender,
    WiFiDirectServiceSessionRequestedEventArgs const& args) {
  try {
    auto request = args.GetSessionRequest();
    if (!request) {
      LOG(ERROR) << "OnSessionRequested returned null session request";
      co_return;
    }
    device_info_ = request.DeviceInformation();
    LOG(INFO) << "GO: OnSessionRequested: "
              << winrt::to_string(device_info_.Id())
              << " Is GroupFormationNeeded: "
              << request.ProvisioningInfo().IsGroupFormationNeeded()
              << ", SelectedConfigurationMethod: "
              << ConfigMethodToString(
                     request.ProvisioningInfo().SelectedConfigurationMethod());

    LOG(INFO) << "GO: Dispatch to UI thread to call ConnectAsync";
    dispatcher_queue_.TryEnqueue([this]() {
      LOG(INFO) << "GO: TryEnqueue: calling ConnectAsync";

      absl::MutexLock lock(mutex_);
      WiFiDirectServiceSession session = nullptr;
      auto pin = credentials_go_->GetPin();
      if (pin.empty()) {
        session = advertiser_.ConnectAsync(device_info_).get();  // NOLINT
      } else {
        session = advertiser_.ConnectAsync(device_info_, winrt::to_hstring(pin))
                      .get();
      }
      LOG(INFO) << "GO: TryEnqueue: Wait for ConnectAsync finish";
      if (!session) {
        LOG(ERROR) << "OnSessionRequested returned null session";
        return;
      }
      LOG(INFO) << "GO: TryEnqueue: OnSessionRequested: ConnectAsync succeeded";
      session_ = std::move(session);

      auto endpoint_pairs = session_.GetConnectionEndpointPairs();
      if (endpoint_pairs.Size() > 0) {
        auto const& pair = endpoint_pairs.GetAt(0);
        ip_address_local_ =
            winrt::to_string(pair.LocalHostName().DisplayName());
        ip_address_remote_ =
            winrt::to_string(pair.RemoteHostName().DisplayName());
        LOG(INFO) << "GO: Local IP: " << ip_address_local_
                  << ", Remote IP: " << ip_address_remote_;
        is_ip_address_ready_.SignalAll();
      } else {
        LOG(WARNING) << "GO: No connection endpoint pairs found.";
      }
      medium_status_ |= kMediumStatusConnected;

      LOG(INFO) << "Service Address: "
                << winrt::to_string(session_.ServiceAddress())
                << ", Service Name: "
                << winrt::to_string(session_.ServiceName())
                << ", Advertisement ID: " << session_.AdvertisementId()
                << ", Session Address: "
                << winrt::to_string(session_.SessionAddress())
                << ", Session ID: " << session_.SessionId();
      // Subscribe to events to prevent early teardown
      session_.SessionStatusChanged([](auto const& s, auto const& e) {
        LOG(INFO) << "GO: TryEnqueue: Session status changed";
      });
    });
    LOG(INFO) << "GO: Dispatch to UI thread to call ConnectAsync finish";
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Failed to get session. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Failed to get session. WinRT exception: " << error.code()
               << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
}

bool WifiDirectMedium::ConnectWifiDirect(
    const WifiDirectCredentials& credentials) {
  LOG(INFO) << "WifiDirectMedium::ConnectWifiDirect";
  absl::MutexLock lock(mutex_);
  if (IsConnecting()) {
    LOG(WARNING) << "Service discovery already running";
    return false;
  }

  if (device_watcher_) {
    LOG(WARNING)
        << "Device Watcher has already been set, please investigate! Skip";
    return false;
  }

  credentials_gc_ = credentials;
  if (credentials_gc_.GetServiceName().empty()) {
    LOG(ERROR) << "GC: Service name is empty, return false";
    return false;
  }
  winrt::hstring device_selector = WiFiDirectService::GetSelector(
      winrt::to_hstring(credentials_gc_.GetServiceName()));
  const winrt::param::iterable<winrt::hstring> requested_properties =
      winrt::single_threaded_vector<winrt::hstring>({
          winrt::to_hstring("System.Devices.WiFiDirectServices.ServiceAddress"),
          winrt::to_hstring("System.Devices.WiFiDirectServices.ServiceName"),
          winrt::to_hstring(
              "System.Devices.WiFiDirectServices.ServiceInformation"),
          winrt::to_hstring(
              "System.Devices.WiFiDirectServices.AdvertisementId"),
          winrt::to_hstring(
              "System.Devices.WiFiDirectServices.ServiceConfigMethods"),
      });
  LOG(INFO) << "Create device watcher";
  device_watcher_ =
      DeviceInformation::CreateWatcher(device_selector, requested_properties);
  device_watcher_added_event_token_ =
      device_watcher_.Added({this, &WifiDirectMedium::Watcher_DeviceAdded});
  device_watcher_updated_event_token_ =
      device_watcher_.Updated({this, &WifiDirectMedium::Watcher_DeviceUpdated});
  device_watcher_removed_event_token_ =
      device_watcher_.Removed({this, &WifiDirectMedium::Watcher_DeviceRemoved});
  device_watcher_enumeration_completed_event_token_ =
      device_watcher_.EnumerationCompleted(
          {this, &WifiDirectMedium::Watcher_DeviceEnumerationCompleted});
  device_watcher_stopped_event_token_ =
      device_watcher_.Stopped({this, &WifiDirectMedium::Watcher_DeviceStopped});
  device_watcher_.Start();
  medium_status_ |= kMediumStatusConnecting;
  LOG(INFO) << "Started to discover WifiDirect service and connect.";
  return true;
}

fire_and_forget WifiDirectMedium::Watcher_DeviceAdded(
    DeviceWatcher sender, DeviceInformation device_info) {
  LOG(INFO) << "Device Service founded for device ID "
            << winrt::to_string(device_info.Id())
            << ";   device name: " << winrt::to_string(device_info.Name());

  auto props = device_info.Properties();
  if (props.HasKey(L"System.Devices.WiFiDirectServices.ServiceName")) {
    winrt::hstring svc_name = winrt::unbox_value<winrt::hstring>(
        props.Lookup(L"System.Devices.WiFiDirectServices.ServiceName"));
    LOG(INFO) << "Discovered service: " << winrt::to_string(svc_name);
  }
  try {
    service_ = co_await WiFiDirectService::FromIdAsync(device_info.Id());
    if (!service_) {
      LOG(ERROR) << "FromIdAsync returned null service";
      co_return;
    }
    LOG(INFO) << "GC: ConnectAsync in Watcher_DeviceAdded";
    service_.PreferGroupOwnerMode(false);

    WiFiDirectServiceSession session = nullptr;
    auto pin = credentials_gc_.GetPin();
    if (pin.empty()) {
      session = service_.ConnectAsync().get();  // NOLINT
    } else {
      auto prov_info = co_await service_.GetProvisioningInfoAsync(
          WiFiDirectServiceConfigurationMethod::PinEntry);

      if (prov_info.IsGroupFormationNeeded()) {
        LOG(INFO) << "GC: Group formation needed";
      } else {
        LOG(INFO) << "GC: Group formation not needed";
      }
      LOG(INFO) << "GC: SelectedConfigurationMethod: "
                << ConfigMethodToString(
                       prov_info.SelectedConfigurationMethod());

      session = service_.ConnectAsync(winrt::to_hstring(pin)).get();
    }

    if (!session) {
      LOG(ERROR) << "GC: ConnectAsync returned null session";
      co_return;
    }
    LOG(INFO) << "GC: ConnectAsync succeeded";
    session_ = std::move(session);

    auto endpoint_pairs = session_.GetConnectionEndpointPairs();
    if (endpoint_pairs.Size() > 0) {
      auto const& pair = endpoint_pairs.GetAt(0);
      ip_address_local_ = winrt::to_string(pair.LocalHostName().DisplayName());
      ip_address_remote_ =
          winrt::to_string(pair.RemoteHostName().DisplayName());
      LOG(INFO) << "GC: Local IP: " << ip_address_local_
                << ", Remote IP: " << ip_address_remote_;
    } else {
      LOG(WARNING) << "GC: No connection endpoint pairs found.";
    }

    LOG(INFO) << "Service Address: "
              << winrt::to_string(session_.ServiceAddress())
              << ", Service Name: " << winrt::to_string(session_.ServiceName())
              << ", Advertisement ID: " << session_.AdvertisementId()
              << ", Session Address: "
              << winrt::to_string(session_.SessionAddress())
              << ", Session ID: " << session_.SessionId();
    {
      absl::MutexLock lock(mutex_);
      is_ip_address_ready_.SignalAll();
    }
    medium_status_ |= kMediumStatusConnected;

    // Subscribe to events to prevent early teardown
    session_.SessionStatusChanged([](auto const& s, auto const& e) {
      LOG(INFO) << "GC: TryEnqueue: Session status changed";
    });
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Failed to resolve WiFiDirectService from Id. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR)
        << __func__
        << ": Failed to resolve WiFiDirectService from Id. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
}

fire_and_forget WifiDirectMedium::Watcher_DeviceUpdated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  VLOG(1) << "WifiDirectMedium::Watcher_DeviceUpdated";
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::Watcher_DeviceRemoved(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  LOG(INFO) << "WifiDirectMedium::Watcher_DeviceRemoved";
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::Watcher_DeviceEnumerationCompleted(
    DeviceWatcher sender, IInspectable inspectable) {
  LOG(INFO) << "WifiDirectMedium::Watcher_DeviceEnumerationCompleted";
  return fire_and_forget();
}

fire_and_forget WifiDirectMedium::Watcher_DeviceStopped(
    DeviceWatcher sender, IInspectable inspectable) {
  medium_status_ &= (~kMediumStatusConnecting);
  return fire_and_forget();
}

bool WifiDirectMedium::DisconnectWifiDirect() {
  LOG(WARNING) << "Stop connecting.";
  absl::MutexLock lock(mutex_);
  if (!IsConnecting()) {
    LOG(WARNING) << "no discovering service to stop.";
    return false;
  }
  try {
    device_watcher_.Stop();
    device_watcher_.Added(device_watcher_added_event_token_);
    device_watcher_.Updated(device_watcher_updated_event_token_);
    device_watcher_.EnumerationCompleted(
        device_watcher_enumeration_completed_event_token_);
    device_watcher_.Removed(device_watcher_removed_event_token_);
    device_watcher_.Stopped(device_watcher_stopped_event_token_);
    medium_status_ &= (~kMediumStatusConnecting);
    medium_status_ &= (~kMediumStatusConnected);
    device_watcher_ = nullptr;
    service_ = nullptr;
    session_ = nullptr;
    ip_address_local_.clear();
    ip_address_remote_.clear();
  return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GC failed. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Stop WifiDirect GC failed. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
  return false;
}

std::vector<WifiDirectMedium::WifiDirectAuthType>
WifiDirectMedium::GetSupportedWifiDirectAuthTypes() const {
  // Windows only supports WifiDirect with Service Discovery, which uses a PIN.
  return {WifiDirectAuthType::WIFI_DIRECT_WITH_PIN};
}

}  // namespace windows
}  // namespace nearby
