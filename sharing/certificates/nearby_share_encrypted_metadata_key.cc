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

#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "sharing/certificates/constants.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    std::vector<uint8_t> salt, std::vector<uint8_t> encrypted_key)
    : salt_(std::move(salt)), encrypted_key_(std::move(encrypted_key)) {
  NL_DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKeySalt, salt_.size());
  NL_DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKey,
               encrypted_key_.size());
}

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    const NearbyShareEncryptedMetadataKey&) = default;

NearbyShareEncryptedMetadataKey& NearbyShareEncryptedMetadataKey::operator=(
    const NearbyShareEncryptedMetadataKey&) = default;

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    NearbyShareEncryptedMetadataKey&&) = default;

NearbyShareEncryptedMetadataKey& NearbyShareEncryptedMetadataKey::operator=(
    NearbyShareEncryptedMetadataKey&&) = default;

NearbyShareEncryptedMetadataKey::~NearbyShareEncryptedMetadataKey() = default;

}  // namespace sharing
}  // namespace nearby
