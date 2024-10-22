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

#include "sharing/advertisement.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {
namespace {

// v1 advertisements:
//   - ParseVersion() --> 0
//
// v2 advertisements:
//   - ParseVersion() --> 1
//   - Backwards compatible; no changes in advertisement data--aside from the
//     version number--or parsing logic compared to v1.
//   - Only used by GmsCore at the moment.
constexpr int kMaxSupportedAdvertisementParsedVersionNumber = 1;

// The bit mask for parsing and writing Version.
constexpr uint8_t kVersionBitmask = 0b111;

// The bit mask for parsing and writing Visibility.
constexpr uint8_t kVisibilityBitmask = 0b1;

// The bit mask for parsing and writing Device Type.
constexpr uint8_t kDeviceTypeBitmask = 0b111;

// The minimum length of a TLV element.
// 1 byte for type and 1 byte for length, which can be 0.
constexpr uint8_t kTlvMinimumLength = 2;
// LINT.IfChange()
enum class TlvTypes : uint8_t {
  kUnknown = 0,
  kQrCode = 1,
  kVendorId = 2,
};
// The length in bytes of the vendor ID in the TLV advertisement.
constexpr uint8_t kVendorIdLength = 1;

// Turns |vendor_id| into a supported vendor ID.
uint8_t ConvertVendorId(uint8_t vendor_id) {
  switch (vendor_id) {
    case 1:
      return 1;
    default:
      return 0;
  }
}
// LINT.ThenChange(//depot/google3/java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/sharing/provider/connections/certificatemanager/Advertisement.java)

const uint8_t kMinimumSize =
    /* Version(3 bits)|Visibility(1 bit)|Device Type(3 bits)|Reserved(1 bits) */
    1 + sharing::Advertisement::kSaltSize +
    sharing::Advertisement::kMetadataEncryptionKeyHashByteSize;

uint8_t ConvertVersion(int version) {
  return static_cast<uint8_t>((version & kVersionBitmask) << 5);
}

uint8_t ConvertDeviceType(ShareTargetType type) {
  return static_cast<uint8_t>((static_cast<int32_t>(type) & kDeviceTypeBitmask)
                              << 1);
}

uint8_t ConvertHasDeviceName(bool hasDeviceName) {
  return static_cast<uint8_t>((hasDeviceName ? 0 : 1) << 4);
}

int ParseVersion(uint8_t b) { return (b >> 5) & kVersionBitmask; }

// The values are in ShareTargetType.
bool IsKnownDeviceValue(int32_t value) {
  switch (value) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      return true;
    default:
      return false;
  }
}

ShareTargetType ParseDeviceType(uint8_t b) {
  int32_t intermediate = static_cast<int32_t>(b >> 1 & kDeviceTypeBitmask);
  if (IsKnownDeviceValue(intermediate)) {
    return static_cast<ShareTargetType>(intermediate);
  }

  return ShareTargetType::kUnknown;
}

bool ParseHasDeviceName(uint8_t b) {
  return ((b >> 4) & kVisibilityBitmask) == 0;
}
}  // namespace

// static
std::unique_ptr<Advertisement> Advertisement::NewInstance(
    std::vector<uint8_t> salt, std::vector<uint8_t> encrypted_metadata_key,
    ShareTargetType device_type, std::optional<std::string> device_name,
    uint8_t vendor_id) {
  if (salt.size() != Advertisement::kSaltSize) {
    NL_LOG(ERROR) << "Failed to create advertisement because the salt did "
                     "not match the expected length "
                  << salt.size();
    return nullptr;
  }

  if (encrypted_metadata_key.size() !=
      Advertisement::kMetadataEncryptionKeyHashByteSize) {
    NL_LOG(ERROR) << "Failed to create advertisement because the encrypted "
                     "metadata key did "
                     "not match the expected length "
                  << encrypted_metadata_key.size();
    return nullptr;
  }

  if (device_name.has_value() && device_name->size() > UINT8_MAX) {
    NL_LOG(ERROR) << "Failed to create advertisement because device name "
                     "was over UINT8_MAX: "
                  << device_name->size();
    return nullptr;
  }

  // Using `new` to access a non-public constructor.
  return std::make_unique<Advertisement>(
      /* version= */ 0, std::move(salt), std::move(encrypted_metadata_key),
      device_type, std::move(device_name), vendor_id);
}

std::vector<uint8_t> Advertisement::ToEndpointInfo() const {
  // We add 3 bytes for vendor ID because of type (1 byte), len (1 byte), and
  // the ID itself (1 byte).
  int size = kMinimumSize + (device_name_.has_value() ? 1 : 0) +
             (device_name_.has_value() ? device_name_->size() : 0) +
             (vendor_id_ != static_cast<uint8_t>(BlockedVendorId::kNone)
                  ? (kTlvMinimumLength + kVendorIdLength)
                  : 0);

  std::vector<uint8_t> endpoint_info;
  endpoint_info.reserve(size);
  endpoint_info.push_back(
      static_cast<uint8_t>(ConvertVersion(version_) |
                           ConvertHasDeviceName(device_name_.has_value()) |
                           ConvertDeviceType(device_type_)));
  endpoint_info.insert(endpoint_info.end(), salt_.begin(), salt_.end());
  endpoint_info.insert(endpoint_info.end(), encrypted_metadata_key_.begin(),
                       encrypted_metadata_key_.end());

  if (device_name_.has_value()) {
    endpoint_info.push_back(static_cast<uint8_t>(device_name_->size() & 0xff));
    endpoint_info.insert(endpoint_info.end(), device_name_->begin(),
                         device_name_->end());
  }

  // Add vendor ID if it is not the default |VendorId::kNone|.
  if (vendor_id_ != static_cast<uint8_t>(BlockedVendorId::kNone)) {
    // Add vendor ID in TLV format.
    endpoint_info.push_back(static_cast<uint8_t>(TlvTypes::kVendorId));
    // Length is 1 byte.
    endpoint_info.push_back(kVendorIdLength);
    // The vendor ID itself.
    endpoint_info.push_back(vendor_id_);
  }

  return endpoint_info;
}

std::unique_ptr<Advertisement> Advertisement::FromEndpointInfo(
    absl::Span<const uint8_t> endpoint_info) {
  if (endpoint_info.size() < kMinimumSize) {
    NL_LOG(ERROR) << "Failed to parse advertisement because it was too short.";
    return nullptr;
  }

  auto iter = endpoint_info.begin();
  uint8_t first_byte = *iter++;

  int version = ParseVersion(first_byte);
  if (version < 0 || version > kMaxSupportedAdvertisementParsedVersionNumber) {
    NL_LOG(ERROR)
        << "Failed to parse advertisement; unsupported version number "
        << version;
    return nullptr;
  }

  bool has_device_name = ParseHasDeviceName(first_byte);
  ShareTargetType device_type = ParseDeviceType(first_byte);

  std::vector<uint8_t> salt(iter, iter + Advertisement::kSaltSize);
  iter += Advertisement::kSaltSize;

  std::vector<uint8_t> encrypted_metadata_key(
      iter, iter + Advertisement::kMetadataEncryptionKeyHashByteSize);
  iter += Advertisement::kMetadataEncryptionKeyHashByteSize;

  std::optional<std::string> optional_device_name;
  if (has_device_name) {
    int device_name_length = 0;
    if (iter != endpoint_info.end()) device_name_length = *iter++ & 0xff;

    if (device_name_length == 0 ||
        (endpoint_info.end() - iter < device_name_length)) {
      NL_LOG(ERROR)
          << "Failed to parse advertisement because the device name did "
             "not match the expected length "
          << device_name_length;
      return nullptr;
    }

    optional_device_name = std::string(iter, iter + device_name_length);
    iter += device_name_length;
  }

  uint8_t vendor_id = static_cast<uint8_t>(BlockedVendorId::kNone);
  while (endpoint_info.end() - iter >= kTlvMinimumLength) {
    // We will parse a TLV element now.
    TlvTypes type = static_cast<TlvTypes>(*iter++);
    uint8_t value_len = *iter++;
    if (endpoint_info.end() - iter < value_len) {
      NL_LOG(ERROR) << "Invalid length when parsing TLV element: " << value_len;
      return nullptr;
    }
    switch (type) {
      case TlvTypes::kVendorId:
        if (value_len != kVendorIdLength) {
          NL_LOG(ERROR) << "Invalid vendor_id_len: " << value_len;
          return nullptr;
        }
        vendor_id = ConvertVendorId(*iter++);
        break;
      case TlvTypes::kQrCode:
        NL_LOG(INFO) << "Found QR code data, skipping.";
        // TODO: b/341984671 - Implement handling for this TLV type.
        iter += value_len;
        break;
      default:
        NL_LOG(ERROR) << "Unknown TLV type: " << static_cast<uint8_t>(type);
        iter += value_len;
        break;
    }
  }

  return Advertisement::NewInstance(
      std::move(salt), std::move(encrypted_metadata_key), device_type,
      std::move(optional_device_name), vendor_id);
}

bool Advertisement::operator==(const Advertisement& other) const {
  return version_ == other.version_ && salt_ == other.salt_ &&
         encrypted_metadata_key_ == other.encrypted_metadata_key_ &&
         device_type_ == other.device_type_ &&
         device_name_ == other.device_name_ && vendor_id_ == other.vendor_id_;
}

// private
Advertisement::Advertisement(int version, std::vector<uint8_t> salt,
                             std::vector<uint8_t> encrypted_metadata_key,
                             ShareTargetType device_type,
                             std::optional<std::string> device_name,
                             uint8_t vendor_id)
    : version_(version),
      salt_(std::move(salt)),
      encrypted_metadata_key_(std::move(encrypted_metadata_key)),
      device_type_(device_type),
      device_name_(std::move(device_name)),
      vendor_id_(vendor_id) {}

}  // namespace sharing
}  // namespace nearby
