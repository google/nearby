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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_UTIL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_UTIL_H_

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/advertisement.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/nearby_sharing_service.h"

namespace nearby::sharing {

// Checks whether having enough disk space for required storage.
//
// device_info - Nearby Share DeviceInfo
// file_path   - The path is to store sharing contents.
// storage_required - required storage space.
bool IsOutOfStorage(nearby::DeviceInfo& device_info,
                    std::filesystem::path file_path, int64_t storage_required);

// Decodes certificate to find MAC address encoded in it.
std::optional<std::vector<uint8_t>> GetBluetoothMacAddressFromCertificate(
    const NearbyShareDecryptedPublicCertificate& certificate);

// Returns device name based on arguments advertisement and certificate.
std::optional<std::string> GetDeviceName(
    const Advertisement& advertisement,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate);

std::string ReceiveSurfaceStateToString(
    NearbySharingService::ReceiveSurfaceState state);

std::string SendSurfaceStateToString(
    NearbySharingService::SendSurfaceState state);

std::string PowerLevelToString(PowerLevel level);

// Return the most stable device identifier with the following priority:
//   1. Hash of Bluetooth MAC address.
//   2. Certificate ID.
//   3. Endpoint ID.
std::string GetDeviceId(
    absl::string_view endpoint_id,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate);

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_UTIL_H_
