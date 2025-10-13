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

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/windows/wifi_direct_service.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {
constexpr std::wstring_view kServiceName = L"QuickShare";
constexpr std::wstring_view kPin = L"1234";
}  // namespace

WifiDirectServiceMedium::WifiDirectServiceMedium() {
  LOG(INFO) << "WifiDirectServiceMedium::WifiDirectServiceMedium";
  // Create a DispatcherQueue for this thread.
  controller_ = winrt::Windows::System::DispatcherQueueController::
      CreateOnDedicatedThread();
  dispatcher_queue_ = controller_.DispatcherQueue();
  if (!dispatcher_queue_) {
    LOG(WARNING) << "Failed to get DispatcherQueue for current thread. "
                    "ConnectAsync might fail if not called from UI thread.";
  }
}

WifiDirectServiceMedium::~WifiDirectServiceMedium() {
  StopWifiDirectService();
  DisconnectWifiDirectService();
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

bool WifiDirectServiceMedium::StartWifiDirectService() {
  LOG(INFO) << "WifiDirectServiceMedium::StartWifiDirectService";

  absl::MutexLock lock(mutex_);
  if (IsServiceStarted()) {
    LOG(WARNING) << "Already started WifiDirect service, skip.";
    return true;
  }

  // Create Advertiser object
  advertiser_ = WiFiDirectServiceAdvertiser(kServiceName);
  advertisement_status_changed_token_ = advertiser_.AdvertisementStatusChanged(
      {this, &WifiDirectServiceMedium::OnAdvertisementStatusChanged});
  auto_accept_session_connected_token_ = advertiser_.AutoAcceptSessionConnected(
      {this, &WifiDirectServiceMedium::OnAutoAcceptSessionConnected});
  session_requested_token_ = advertiser_.SessionRequested(
      {this, &WifiDirectServiceMedium::OnSessionRequested});

  advertiser_.AutoAcceptSession(false);
  advertiser_.PreferGroupOwnerMode(true);
  advertiser_.ServiceStatus(WiFiDirectServiceStatus::Available);
  // Config Methods
  WiFiDirectServiceConfigurationMethod config_method;
  if (kPin.empty()) {
    config_method = WiFiDirectServiceConfigurationMethod::Default; // NOLINT
  } else {
    config_method = WiFiDirectServiceConfigurationMethod::PinDisplay;
  }
  advertiser_.PreferredConfigurationMethods().Clear();
  advertiser_.PreferredConfigurationMethods().Append(config_method);

  try {
    advertiser_.Start();
    LOG(INFO) << "Start WifiDirect GO succeeded. Status: "
              << (int)advertiser_.AdvertisementStatus();
    if ((advertiser_.AdvertisementStatus() ==
         WiFiDirectServiceAdvertisementStatus::Created) ||
        (advertiser_.AdvertisementStatus() ==
         WiFiDirectServiceAdvertisementStatus::Started)) {
      medium_status_ |= kMediumStatusServiceStarted;
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

bool WifiDirectServiceMedium::StopWifiDirectService() {
  LOG(INFO) << "WifiDirectServiceMedium::StopWifiDirectService";
  absl::MutexLock lock(mutex_);
  if (!IsServiceStarted()) {
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
    medium_status_ &= (~kMediumStatusServiceStarted);
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

std::string WifiDirectServiceMedium::ConfigMethodToString(
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

fire_and_forget WifiDirectServiceMedium::OnAdvertisementStatusChanged(
    WiFiDirectServiceAdvertiser sender, IInspectable const& event) {
  LOG(INFO) << "WiFiDirectServiceAdvertiser status changed: "
            << (int)sender.AdvertisementStatus();
  auto status = sender.ServiceStatus();
  switch (status) {
    case WiFiDirectServiceStatus ::Available:
      LOG(INFO) << "WiFiDirectServiceAdvertiser service status changed: "
                   "status: Available";
      break;
    case WiFiDirectServiceStatus ::Busy:
      LOG(INFO) << "WiFiDirectServiceAdvertiser service status changed: "
                   "status: Busy";
      break;
    case WiFiDirectServiceStatus ::Custom:
      LOG(INFO) << "WiFiDirectServiceAdvertiser service status changed: "
                   "status: Custom";
      break;
    default:
      LOG(INFO) << "WiFiDirectServiceAdvertiser service status changed: "
                   "Code: "
                << (int)status;
      break;
  }
  return winrt::fire_and_forget();
}

fire_and_forget WifiDirectServiceMedium::OnAutoAcceptSessionConnected(
    WiFiDirectServiceAdvertiser sender,
    WiFiDirectServiceAutoAcceptSessionConnectedEventArgs const& args) {
  LOG(INFO) << "WifiDirectServiceMedium::OnAutoAcceptSessionConnected";
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

fire_and_forget WifiDirectServiceMedium::OnSessionRequested(
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
      if (kPin.empty()) {
        session = advertiser_.ConnectAsync(device_info_).get(); // NOLINT
      } else {
        session = advertiser_.ConnectAsync(device_info_, kPin).get();
      }
      LOG(INFO) << "GO: TryEnqueue: Wait for ConnectAsync finish";
      if (!session) {
        LOG(ERROR) << "OnSessionRequested returned null session";
        return;
      }
      LOG(INFO) << "GO: TryEnqueue: OnSessionRequested: ConnectAsync succeeded";
      session_ = std::move(session);
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
    LOG(INFO) << "GC: Dispatch to UI thread to call ConnectAsync finish";
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

bool WifiDirectServiceMedium::ConnectWifiDirectService() {
  LOG(INFO) << "WifiDirectServiceMedium::ConnectWifiDirectService";
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

  discovered_devices_by_id_.clear();
  connection_requested_devices_by_id_.clear();

  winrt::hstring device_selector = WiFiDirectService::GetSelector(kServiceName);
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
  device_watcher_added_event_token_ = device_watcher_.Added(
      {this, &WifiDirectServiceMedium::Watcher_DeviceAdded});
  device_watcher_updated_event_token_ = device_watcher_.Updated(
      {this, &WifiDirectServiceMedium::Watcher_DeviceUpdated});
  device_watcher_removed_event_token_ = device_watcher_.Removed(
      {this, &WifiDirectServiceMedium::Watcher_DeviceRemoved});
  device_watcher_enumeration_completed_event_token_ =
      device_watcher_.EnumerationCompleted(
          {this, &WifiDirectServiceMedium::Watcher_DeviceEnumerationCompleted});
  device_watcher_stopped_event_token_ = device_watcher_.Stopped(
      {this, &WifiDirectServiceMedium::Watcher_DeviceStopped});
  device_watcher_.Start();
  medium_status_ |= kMediumStatusConnecting;
  LOG(INFO) << "Started to discover WifiDirect service and connect.";
  return true;
}

fire_and_forget WifiDirectServiceMedium::Watcher_DeviceAdded(
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
    if (kPin.empty()) {
      session = service_.ConnectAsync().get(); // NOLINT
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

      session = service_.ConnectAsync(kPin).get();
    }

    if (!session) {
      LOG(ERROR) << "GC: ConnectAsync returned null session";
      co_return;
    }
    LOG(INFO) << "GC: ConnectAsync succeeded";
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

fire_and_forget WifiDirectServiceMedium::Watcher_DeviceUpdated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  VLOG(1) << "WifiDirectServiceMedium::Watcher_DeviceUpdated";
  return fire_and_forget();
}

fire_and_forget WifiDirectServiceMedium::Watcher_DeviceRemoved(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  LOG(INFO) << "WifiDirectServiceMedium::Watcher_DeviceRemoved";
  return fire_and_forget();
}

fire_and_forget WifiDirectServiceMedium::Watcher_DeviceEnumerationCompleted(
    DeviceWatcher sender, IInspectable inspectable) {
  LOG(INFO) << "WifiDirectServiceMedium::Watcher_DeviceEnumerationCompleted";
  return fire_and_forget();
}

fire_and_forget WifiDirectServiceMedium::Watcher_DeviceStopped(
    DeviceWatcher sender, IInspectable inspectable) {
  LOG(INFO) << "WifiDirectServiceMedium::Watcher_DeviceStopped";
  return fire_and_forget();
}

bool WifiDirectServiceMedium::DisconnectWifiDirectService() {
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
    device_watcher_ = nullptr;
    service_ = nullptr;
    session_ = nullptr;
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

}  // namespace windows
}  // namespace nearby
