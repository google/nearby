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

#include "location/nearby/cpp/sharing/clients/windows/app/app_controller.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>  // NOLINT
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/platform/logging.h"
#include "sharing/advertisement.h"
#include "sharing/attachment_container.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/file_attachment.h"
#include "sharing/nearby_sharing_service_factory.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"
#include "sharing/wifi_credentials_attachment.h"
#include "winrt/base.h"

namespace nearby::sharing::windows_app {
namespace {

using ::nearby::sharing::proto::DeviceVisibility;
using StatusCodes = NearbySharingService::StatusCodes;

void LogStatus(const char* operation, StatusCodes status) {
  if (status != StatusCodes::kOk) {
    LOG(WARNING) << operation << " failed: "
                 << NearbySharingService::StatusCodeToString(status);
  }
}

std::string Summarize(const AttachmentContainer& container) {
  size_t files = container.GetFileAttachments().size();
  size_t texts = container.GetTextAttachments().size();
  size_t wifi = container.GetWifiCredentialsAttachments().size();
  if (files == 1 && texts == 0 && wifi == 0) {
    return std::string(container.GetFileAttachments()[0].file_name());
  }
  if (files == 0 && texts == 1 && wifi == 0) {
    return std::string(container.GetTextAttachments()[0].text_title());
  }
  if (files == 0 && texts == 0 && wifi == 1) {
    return absl::StrCat("Wi-Fi network ",
                        container.GetWifiCredentialsAttachments()[0].ssid());
  }
  std::string summary;
  if (files > 0) absl::StrAppend(&summary, files, files == 1 ? " file" : " files");
  if (texts > 0) {
    absl::StrAppend(&summary, summary.empty() ? "" : ", ", texts,
                    texts == 1 ? " text" : " texts");
  }
  if (wifi > 0) {
    absl::StrAppend(&summary, summary.empty() ? "" : ", ", wifi,
                    " Wi-Fi network", wifi == 1 ? "" : "s");
  }
  return summary;
}

WifiNetwork::Security ToSecurity(
    WifiCredentialsAttachment::SecurityType security_type) {
  switch (security_type) {
    case WifiCredentialsAttachment::SecurityType::
        WifiCredentialsMetadata_SecurityType_OPEN:
      return WifiNetwork::Security::kOpen;
    case WifiCredentialsAttachment::SecurityType::
        WifiCredentialsMetadata_SecurityType_WPA_PSK:
      return WifiNetwork::Security::kWpaPsk;
    case WifiCredentialsAttachment::SecurityType::
        WifiCredentialsMetadata_SecurityType_WEP:
      return WifiNetwork::Security::kWep;
    case WifiCredentialsAttachment::SecurityType::
        WifiCredentialsMetadata_SecurityType_SAE:
      return WifiNetwork::Security::kSae;
    default:
      return WifiNetwork::Security::kUnknown;
  }
}

}  // namespace

AppController::AppController(HWND ui_window, Listener* listener)
    : ui_window_(ui_window), listener_(listener) {
  // C++/WinRT forbids blocking waits on the STA UI thread, and the platform
  // waits on WinRT operations while it starts, so create it in the MTA.
  std::thread([this] {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    platform_ = std::make_unique<sharing::windows::WindowsPlatform>();
    analytics_recorder_ = std::make_unique<analytics::AnalyticsRecorderImpl>(
        /*vendor_id=*/0, /*event_logger=*/nullptr);
    service_ = NearbySharingServiceFactory::GetInstance()->CreateSharingService(
        *platform_, analytics_recorder_.get(), /*event_logger=*/nullptr,
        /*supports_file_sync=*/false);
  }).join();

  service_->RegisterReceiveSurface(
      &receive_observer_, NearbySharingService::ReceiveSurfaceState::kBackground,
      Advertisement::BlockedVendorId::kNone,
      [](StatusCodes status) { LogStatus("RegisterReceiveSurface", status); });
}

AppController::~AppController() {
  absl::Notification shutdown;
  service_->Shutdown([&shutdown](StatusCodes status) {
    LogStatus("Shutdown", status);
    shutdown.Notify();
  });
  shutdown.WaitForNotificationWithTimeout(absl::Seconds(10));
}

std::string AppController::GetDeviceName() const {
  return service_->GetSettings()->GetDeviceName();
}

bool AppController::SetDeviceName(const std::string& name) {
  // The settings validate synchronously before saving.
  bool valid = false;
  service_->GetSettings()->SetDeviceName(
      name, [&valid](DeviceNameValidationResult result) {
        valid = result == DeviceNameValidationResult::kValid;
      });
  return valid;
}

std::wstring AppController::GetSaveFolder() const {
  return FilePath(service_->GetSettings()->GetCustomSavePath())
      .ToWideString();
}

void AppController::SetSaveFolder(const std::wstring& folder) {
  service_->GetSettings()->SetCustomSavePathAsync(FilePath(folder).ToString(),
                                                  [] {});
}

VisibilityMode AppController::GetVisibilityMode() const {
  NearbyShareSettings* settings = service_->GetSettings();
  if (settings->GetVisibility() !=
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE) {
    return VisibilityMode::kHidden;
  }
  // Temporary visibility keeps a fallback to restore when it expires.
  return settings->GetFallbackVisibility().visibility ==
                 DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED
             ? VisibilityMode::kEveryone
             : VisibilityMode::kEveryoneFor10Minutes;
}

void AppController::SetVisibilityMode(VisibilityMode mode) {
  service_->SetVisibility(
      mode == VisibilityMode::kHidden
          ? DeviceVisibility::DEVICE_VISIBILITY_HIDDEN
          : DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      mode == VisibilityMode::kEveryoneFor10Minutes ? absl::Minutes(10)
                                                    : absl::ZeroDuration(),
      [](StatusCodes status) { LogStatus("SetVisibility", status); });
}

void AppController::SetReceivingInForeground(bool foreground) {
  // A transfer callback is registered for one state at a time.
  service_->UnregisterReceiveSurface(&receive_observer_, [](StatusCodes) {});
  service_->RegisterReceiveSurface(
      &receive_observer_,
      foreground ? NearbySharingService::ReceiveSurfaceState::kForeground
                 : NearbySharingService::ReceiveSurfaceState::kBackground,
      Advertisement::BlockedVendorId::kNone,
      [](StatusCodes status) { LogStatus("RegisterReceiveSurface", status); });
}

void AppController::StartSending(std::vector<std::wstring> files) {
  bool already_sending = IsSending();
  pending_files_ = std::move(files);
  if (already_sending) return;
  devices_.clear();
  listener_->OnNearbyDevicesChanged(devices_);
  service_->RegisterSendSurface(
      &send_observer_, &discovery_observer_,
      NearbySharingService::SendSurfaceState::kForeground,
      Advertisement::BlockedVendorId::kNone, /*disable_wifi_hotspot=*/false,
      [](StatusCodes status) { LogStatus("RegisterSendSurface", status); });
}

void AppController::StopSending() {
  if (!IsSending()) return;
  pending_files_.clear();
  devices_.clear();
  listener_->OnNearbyDevicesChanged(devices_);
  service_->UnregisterSendSurface(&send_observer_, [](StatusCodes status) {
    LogStatus("UnregisterSendSurface", status);
  });
}

void AppController::SendTo(int64_t share_target_id) {
  if (!IsSending()) return;
  AttachmentContainer::Builder builder;
  for (const std::wstring& file : pending_files_) {
    builder.AddFileAttachment(FileAttachment(FilePath(file)));
  }
  service_->SendAttachments(
      share_target_id, builder.Build(),
      [](StatusCodes status) { LogStatus("SendAttachments", status); });
}

void AppController::Accept(int64_t share_target_id) {
  service_->Accept(share_target_id,
                   [](StatusCodes status) { LogStatus("Accept", status); });
}

void AppController::Reject(int64_t share_target_id) {
  service_->Reject(share_target_id,
                   [](StatusCodes status) { LogStatus("Reject", status); });
}

void AppController::Cancel(int64_t share_target_id) {
  service_->Cancel(share_target_id,
                   [](StatusCodes status) { LogStatus("Cancel", status); });
}

void AppController::PostToUi(std::function<void()> task) {
  auto* heap_task = new std::function<void()>(std::move(task));
  if (!PostMessageW(ui_window_, kRunOnUiThreadMessage, 0,
                    reinterpret_cast<LPARAM>(heap_task))) {
    delete heap_task;
  }
}

void AppController::UpdateDevice(const ShareTarget& share_target, bool lost) {
  NearbyDevice device{share_target.id, share_target.device_name};
  PostToUi([this, device, lost] {
    auto it = std::find_if(devices_.begin(), devices_.end(),
                           [&](const auto& d) { return d.id == device.id; });
    if (lost) {
      if (it != devices_.end()) devices_.erase(it);
    } else if (it != devices_.end()) {
      *it = device;
    } else {
      devices_.push_back(device);
    }
    listener_->OnNearbyDevicesChanged(devices_);
  });
}

void AppController::TransferObserver::OnTransferUpdate(
    const ShareTarget& share_target,
    const AttachmentContainer& attachment_container,
    const TransferMetadata& transfer_metadata) {
  TransferView view;
  view.share_target_id = share_target.id;
  view.device_name = share_target.device_name;
  view.incoming = incoming_;
  view.status = transfer_metadata.status();
  view.progress = transfer_metadata.progress();
  view.is_final = transfer_metadata.is_final_status();
  view.token = transfer_metadata.token();
  view.summary = Summarize(attachment_container);
  for (const FileAttachment& file :
       attachment_container.GetFileAttachments()) {
    if (file.file_path().has_value()) {
      view.file_paths.push_back(file.file_path()->ToWideString());
    }
  }
  for (const auto& text : attachment_container.GetTextAttachments()) {
    view.texts.emplace_back(text.text_body());
  }
  for (const auto& wifi : attachment_container.GetWifiCredentialsAttachments()) {
    view.wifi_networks.push_back(WifiNetwork{
        .ssid = std::string(wifi.ssid()),
        .password = std::string(wifi.password()),
        .security = ToSecurity(wifi.security_type()),
        .hidden = wifi.is_hidden(),
    });
  }
  controller_.PostToUi([&controller = controller_, view = std::move(view)] {
    controller.listener_->OnTransferUpdated(view);
  });
}

void AppController::DiscoveryObserver::OnShareTargetDiscovered(
    const ShareTarget& share_target) {
  controller_.UpdateDevice(share_target, /*lost=*/false);
}

void AppController::DiscoveryObserver::OnShareTargetLost(
    const ShareTarget& share_target) {
  controller_.UpdateDevice(share_target, /*lost=*/true);
}

void AppController::DiscoveryObserver::OnShareTargetUpdated(
    const ShareTarget& share_target) {
  controller_.UpdateDevice(share_target, /*lost=*/false);
}

}  // namespace nearby::sharing::windows_app
