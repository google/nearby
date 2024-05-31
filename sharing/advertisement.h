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

#ifndef THIRD_PARTY_NEARBY_SHARING_ADVERTISEMENT_H_
#define THIRD_PARTY_NEARBY_SHARING_ADVERTISEMENT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "sharing/common/nearby_share_enums.h"

namespace nearby {
namespace sharing {

// An advertisement in the form of
// [VERSION|VISIBILITY][SALT][ACCOUNT_IDENTIFIER][LEN][DEVICE_NAME].
// A device name indicates the advertisement is visible to everyone;
// a missing device name indicates the advertisement is contacts-only.
class Advertisement {
 public:
  static constexpr uint8_t kSaltSize = 2;
  static constexpr uint8_t kMetadataEncryptionKeyHashByteSize = 14;
  // LINT.IfChange()
  // Lists supported vendors for target blocking.
  enum class BlockedVendorId : uint8_t {
    kNone = 0,
    kSamsung = 1,
  };
  // LINT.ThenChange(//depot/google3/java/com/google/android/gmscore/integ/client/nearby/src/com/google/android/gms/nearby/sharing/SharingOptions.java:VendorId)

  static std::unique_ptr<Advertisement> NewInstance(
      std::vector<uint8_t> salt, std::vector<uint8_t> encrypted_metadata_key,
      ShareTargetType device_type, std::optional<std::string> device_name,
      uint8_t vendor_id);

  // TODO: b/341967036 - Remove uses of std::optional for device name. Empty
  // string should be enough.
  Advertisement(int version, std::vector<uint8_t> salt,
                std::vector<uint8_t> encrypted_metadata_key,
                ShareTargetType device_type,
                std::optional<std::string> device_name, uint8_t vendor_id);
  ~Advertisement() = default;
  Advertisement(const Advertisement&) = default;
  Advertisement& operator=(const Advertisement&) = default;
  Advertisement(Advertisement&&) = default;
  Advertisement& operator=(Advertisement&&) = default;
  bool operator==(const Advertisement& other) const;

  std::vector<uint8_t> ToEndpointInfo() const;

  int version() const { return version_; }
  const std::vector<uint8_t>& salt() const { return salt_; }
  const std::vector<uint8_t>& encrypted_metadata_key() const {
    return encrypted_metadata_key_;
  }
  ShareTargetType device_type() const { return device_type_; }
  const std::optional<std::string>& device_name() const { return device_name_; }
  bool HasDeviceName() const { return device_name_.has_value(); }
  uint8_t vendor_id() const { return vendor_id_; }

  static std::unique_ptr<Advertisement> FromEndpointInfo(
      absl::Span<const uint8_t> endpoint_info);

 private:
  // The version of the advertisement. Different versions can have different
  // ways of parsing the endpoint id.
  int version_;

  // Random bytes that were used as salt during encryption of public certificate
  // metadata.
  std::vector<uint8_t> salt_ = {};

  // An encrypted symmetric key that was used to encrypt public certificate
  // metadata, including an account identifier signifying the remote device.
  // The key can be decrypted using |salt| and the corresponding public
  // certificate's secret/authenticity key.
  std::vector<uint8_t> encrypted_metadata_key_ = {};

  // The type of device that the advertisement identifies.
  ShareTargetType device_type_ = ShareTargetType::kUnknown;

  // The human-readable name of the remote device.
  std::optional<std::string> device_name_ = std::nullopt;

  // The vendor identifier of the remote device. Reference for vendor ID:
  // google3/java/com/google/android/gmscore/integ/client/nearby/src/com/google/android/gms/nearby/sharing/SharingOptions.java
  const uint8_t vendor_id_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_ADVERTISEMENT_H_
