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

#include "sharing/certificates/nearby_share_certificate_manager.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "absl/types/span.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::proto::DeviceVisibility;

NearbyShareCertificateManager::NearbyShareCertificateManager() = default;

NearbyShareCertificateManager::~NearbyShareCertificateManager() = default;

void NearbyShareCertificateManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareCertificateManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareCertificateManager::Start() {
  if (is_running_) return;

  is_running_ = true;
  OnStart();
}

void NearbyShareCertificateManager::Stop() {
  if (!is_running_) return;

  is_running_ = false;
  OnStop();
}

std::optional<NearbyShareEncryptedMetadataKey>
NearbyShareCertificateManager::EncryptPrivateCertificateMetadataKey(
    DeviceVisibility visibility) {
  std::optional<NearbySharePrivateCertificate> cert =
      GetValidPrivateCertificate(visibility);
  if (!cert) return std::nullopt;

  std::optional<NearbyShareEncryptedMetadataKey> encrypted_key =
      cert->EncryptMetadataKey();

  // Every salt consumed to encrypt the metadata encryption key is tracked by
  // the NearbySharePrivateCertificate. Update the private certificate in
  // storage to reflect the new list of consumed salts.
  UpdatePrivateCertificateInStorage(*cert);

  return encrypted_key;
}

std::optional<std::vector<uint8_t>>
NearbyShareCertificateManager::SignWithPrivateCertificate(
    DeviceVisibility visibility, absl::Span<const uint8_t> payload) const {
  std::optional<NearbySharePrivateCertificate> cert =
      GetValidPrivateCertificate(visibility);
  if (!cert) return std::nullopt;

  return cert->Sign(payload);
}

std::optional<std::vector<uint8_t>>
NearbyShareCertificateManager::HashAuthenticationTokenWithPrivateCertificate(
    DeviceVisibility visibility,
    absl::Span<const uint8_t> authentication_token) const {
  std::optional<NearbySharePrivateCertificate> cert =
      GetValidPrivateCertificate(visibility);
  if (!cert) return std::nullopt;

  return cert->HashAuthenticationToken(authentication_token);
}

void NearbyShareCertificateManager::NotifyPublicCertificatesDownloaded() {
  for (const auto& observer : observers_.GetObservers())
    observer->OnPublicCertificatesDownloaded();
}

void NearbyShareCertificateManager::NotifyPrivateCertificatesChanged() {
  for (const auto& observer : observers_.GetObservers())
    observer->OnPrivateCertificatesChanged();
}

}  // namespace sharing
}  // namespace nearby
