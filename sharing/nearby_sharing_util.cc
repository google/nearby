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

#include "sharing/nearby_sharing_util.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>  // NOLINT(build/c++17)
#include <optional>
#include <string>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/advertisement.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/base/encode.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_sharing_service.h"

namespace nearby::sharing {

std::string ReceiveSurfaceStateToString(
    NearbySharingService::ReceiveSurfaceState state) {
  switch (state) {
    case NearbySharingService::ReceiveSurfaceState::kForeground:
      return "FOREGROUND";
    case NearbySharingService::ReceiveSurfaceState::kBackground:
      return "BACKGROUND";
    case NearbySharingService::ReceiveSurfaceState::kUnknown:
      return "UNKNOWN";
  }
}

std::string SendSurfaceStateToString(
    NearbySharingService::SendSurfaceState state) {
  switch (state) {
    case NearbySharingService::SendSurfaceState::kForeground:
      return "FOREGROUND";
    case NearbySharingService::SendSurfaceState::kBackground:
      return "BACKGROUND";
    case NearbySharingService::SendSurfaceState::kUnknown:
      return "UNKNOWN";
  }
}

std::string PowerLevelToString(PowerLevel level) {
  switch (level) {
    case PowerLevel::kLowPower:
      return "LOW_POWER";
    case PowerLevel::kMediumPower:
      return "MEDIUM_POWER";
    case PowerLevel::kHighPower:
      return "HIGH_POWER";
    case PowerLevel::kUnknown:
      return "UNKNOWN";
  }
}

std::optional<std::vector<uint8_t>> GetBluetoothMacAddressFromCertificate(
    const NearbyShareDecryptedPublicCertificate& certificate) {
  if (!certificate.unencrypted_metadata().has_bluetooth_mac_address()) {
    NL_LOG(WARNING) << __func__ << ": Public certificate "
                    << nearby::utils::HexEncode(certificate.id())
                    << " did not contain a Bluetooth mac address.";
    return std::nullopt;
  }

  std::string mac_address =
      certificate.unencrypted_metadata().bluetooth_mac_address();
  if (mac_address.size() != 6) {
    NL_LOG(ERROR) << __func__ << ": Invalid bluetooth mac address: '"
                  << mac_address << "'";
    return std::nullopt;
  }

  return std::vector<uint8_t>(mac_address.begin(), mac_address.end());
}

std::optional<std::string> GetDeviceName(
    const Advertisement& advertisement,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate) {

  // Device name is always included when visible to everyone.
  if (advertisement.device_name().has_value()) {
    return advertisement.device_name();
  }

  // For contacts only advertisements, we can't do anything without the
  // certificate.
  if (!certificate.has_value() ||
      !certificate->unencrypted_metadata().has_device_name()) {
    return std::nullopt;
  }

  return certificate->unencrypted_metadata().device_name();
}

// Return the most stable device identifier with the following priority:
//   1. Hash of Bluetooth MAC address.
//   2. Certificate ID.
//   3. Endpoint ID.
std::string GetDeviceId(
    absl::string_view endpoint_id,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate) {
  if (!certificate.has_value()) {
    return std::string(endpoint_id);
  }

  std::optional<std::vector<uint8_t>> mac_address =
      GetBluetoothMacAddressFromCertificate(*certificate);
  if (mac_address.has_value()) {
    return absl::StrCat(absl::Hash<std::vector<uint8_t>>{}(*mac_address));
  }

  if (!certificate->id().empty()) {
    return std::string(certificate->id().begin(), certificate->id().end());
  }

  return std::string(endpoint_id);
}

bool IsOutOfStorage(DeviceInfo& device_info, std::filesystem::path file_path,
                    int64_t storage_required) {
  std::optional<size_t> available_storage =
      device_info.GetAvailableDiskSpaceInBytes(file_path);

  if (!available_storage.has_value()) {
    return false;
  }

  return *available_storage <= storage_required;
}

}  // namespace nearby::sharing
