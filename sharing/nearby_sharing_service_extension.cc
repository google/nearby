// Copyright 2023 Google LLC
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

#include "sharing/nearby_sharing_service_extension.h"

#include <filesystem>  // NOLINT(build/c++17)
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "internal/network/url.h"
#include "sharing/file_attachment.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/text_attachment.h"
#include "sharing/wifi_credentials_attachment.h"

namespace nearby {
namespace sharing {

namespace {
using ::nearby::sharing::service::proto::FileMetadata;
using ::nearby::sharing::service::proto::TextMetadata;
using StatusCodes = ::nearby::sharing::NearbySharingService::StatusCodes;
}  // namespace

NearbySharingService::StatusCodes NearbySharingServiceExtension::Open(
    const ShareTarget& share_target) {
  if (share_target.file_attachments.empty() &&
      share_target.text_attachments.empty() &&
      share_target.wifi_credentials_attachments.empty()) {
    return StatusCodes::kOk;
  }

  if (!share_target.file_attachments.empty() &&
      !share_target.text_attachments.empty()) {
    NL_LOG(ERROR)
        << __func__
        << ": Text attachments and file attachments can't come together.";
    return StatusCodes::kError;
  }

  if (share_target.text_attachments.size() == 1) {
    const TextAttachment& text_attachment = share_target.text_attachments[0];

    switch (text_attachment.type()) {
      case TextMetadata::TEXT: {
        CopyText(text_attachment.text_body());
        break;
      }
      case TextMetadata::URL: {
        OpenUrl(*nearby::network::Url::Create(text_attachment.text_body()));
        break;
      }
      default: {
        // Copy text for all other text types.
        CopyText(text_attachment.text_title());
        break;
      }
    }
    return StatusCodes::kOk;
  }

  if (share_target.text_attachments.size() > 1) {
    NL_LOG(ERROR) << __func__
                  << ": Multiple text attachments are not supported currently.";
    return StatusCodes::kError;
  }

  if (share_target.wifi_credentials_attachments.size() == 1) {
    const WifiCredentialsAttachment& wifi_credentials_attachment =
        share_target.wifi_credentials_attachments[0];
    JoinWifiNetwork(wifi_credentials_attachment.ssid(),
                    wifi_credentials_attachment.password());
    return StatusCodes::kOk;
  }

  if (share_target.wifi_credentials_attachments.size() > 1) {
    NL_LOG(ERROR) << __func__
                  << ": Multiple WiFi credentials attachments are not "
                     "supported currently.";
    return StatusCodes::kError;
  }

  const FileAttachment& file_attachment = share_target.file_attachments[0];

  if ((share_target.file_attachments.size() > 1) ||
      ((file_attachment.type() != FileMetadata::AUDIO) &&
       (file_attachment.type() != FileMetadata::VIDEO) &&
       (file_attachment.type() != FileMetadata::IMAGE))) {
    // Opens download folder.
    NearbySharingService::StatusCodes status_codes = StatusCodes::kOk;
    absl::Notification notification;
    context_->GetShell().Open(
        std::filesystem::u8path(settings_->GetCustomSavePath()),
        [&status_codes, &notification](absl::Status status) {
          if (!status.ok()) {
            NL_LOG(ERROR)
                << "Failed to open download folder with error message:"
                << status;
            status_codes = StatusCodes::kError;
          } else {
            status_codes = StatusCodes::kOk;
          }
          notification.Notify();
        });
    notification.WaitForNotification();
    return status_codes;
  }

  // Opens the file with default application.
  std::filesystem::path file_path;
  if (file_attachment.file_path().has_value()) {
    file_path = *file_attachment.file_path();
  } else {
    file_path =
        std::filesystem::u8path(settings_->GetCustomSavePath()) /
        // NOLINTNEXTLINE cannot build without the new string creation
        std::filesystem::u8path(std::string(file_attachment.file_name()));
  }

  NearbySharingService::StatusCodes status_codes = StatusCodes::kOk;
  absl::Notification notification;
  context_->GetShell().Open(
      file_path, [file_name = file_attachment.file_name(), &status_codes,
                  &notification](absl::Status status) {
        if (!status.ok()) {
          NL_LOG(ERROR) << "Failed to open file " << file_name;
          status_codes = StatusCodes::kError;
        } else {
          status_codes = StatusCodes::kOk;
        }
        notification.Notify();
      });
  notification.WaitForNotification();
  return status_codes;
}

// Opens an url target on a browser instance.
void NearbySharingServiceExtension::OpenUrl(const ::nearby::network::Url& url) {
  absl::Notification notification;
  context_->OpenUrl(url, [url, &notification](absl::Status status) {
    if (!status.ok()) {
      NL_LOG(ERROR) << "Failed to open URL " << url.GetUrlPath()
                    << " with error " << status.message();
    }
    notification.Notify();
  });

  notification.WaitForNotification();
}

// Copies text to cache/clipboard.
void NearbySharingServiceExtension::CopyText(absl::string_view text) {
  absl::Notification notification;
  context_->CopyText(
      text, [text = std::string(text), &notification](absl::Status status) {
        if (!status.ok()) {
          NL_LOG(ERROR) << "Failed to copy text " << text << " with error "
                        << status.message();
        }
        notification.Notify();
      });
  notification.WaitForNotification();
}

// Persists and joins the Wi-Fi network.
void NearbySharingServiceExtension::JoinWifiNetwork(
    absl::string_view ssid, absl::string_view password) {
  absl::Notification notification;
  context_->GetWifiAdapter().JoinNetwork(
      ssid, password,
      [ssid = std::string(ssid), &notification](absl::Status status) {
        if (!status.ok()) {
          NL_LOG(ERROR) << "Failed to join network " << ssid << " with error "
                        << status.message();
        }
        notification.Notify();
      });
  notification.WaitForNotification();
}

}  // namespace sharing
}  // namespace nearby
