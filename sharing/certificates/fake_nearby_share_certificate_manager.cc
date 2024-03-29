// Copyright 2022-2023 Google LLC
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

#include "sharing/certificates/fake_nearby_share_certificate_manager.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::proto::DeviceVisibility;

class NearbyShareClientFactory;

FakeNearbyShareCertificateManager::Factory::Factory() = default;

FakeNearbyShareCertificateManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareCertificateManager>
FakeNearbyShareCertificateManager::Factory::CreateInstance(
    nearby::Context* context,
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    NearbyShareContactManager* contact_manager, absl::string_view profile_path,
    nearby::sharing::api::SharingRpcClientFactory* client_factory) {
  auto instance = std::make_unique<FakeNearbyShareCertificateManager>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::
    GetDecryptedPublicCertificateCall(
        NearbyShareEncryptedMetadataKey encrypted_metadata_key,
        CertDecryptedCallback callback)
    : encrypted_metadata_key(std::move(encrypted_metadata_key)),
      callback(std::move(callback)) {}

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::
    GetDecryptedPublicCertificateCall(
        GetDecryptedPublicCertificateCall&& other) = default;

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall&
FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::operator=(
    GetDecryptedPublicCertificateCall&& other) = default;

FakeNearbyShareCertificateManager::GetDecryptedPublicCertificateCall::
    ~GetDecryptedPublicCertificateCall() = default;

FakeNearbyShareCertificateManager::FakeNearbyShareCertificateManager()
    : next_salt_(GetNearbyShareTestSalt()) {}

FakeNearbyShareCertificateManager::~FakeNearbyShareCertificateManager() =
    default;

std::vector<nearby::sharing::proto::PublicCertificate>
FakeNearbyShareCertificateManager::GetPrivateCertificatesAsPublicCertificates(
    DeviceVisibility visibility) {
  ++num_get_private_certificates_as_public_certificates_calls_;
  return GetNearbyShareTestPublicCertificateList(visibility);
}

void FakeNearbyShareCertificateManager::GetDecryptedPublicCertificate(
    NearbyShareEncryptedMetadataKey encrypted_metadata_key,
    CertDecryptedCallback callback) {
  get_decrypted_public_certificate_calls_.emplace_back(encrypted_metadata_key,
                                                       std::move(callback));
}

void FakeNearbyShareCertificateManager::DownloadPublicCertificates() {
  ++num_download_public_certificates_calls_;
}

void FakeNearbyShareCertificateManager::ClearPublicCertificates(
    std::function<void(bool)> callback) {
  ++num_clear_public_certificates_calls_;
  callback(true);
}

void FakeNearbyShareCertificateManager::OnStart() {}

void FakeNearbyShareCertificateManager::OnStop() {}

std::optional<NearbySharePrivateCertificate>
FakeNearbyShareCertificateManager::GetValidPrivateCertificate(
    DeviceVisibility visibility) const {
  if (visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE ||
      visibility == DeviceVisibility::DEVICE_VISIBILITY_HIDDEN) {
    return std::nullopt;
  }
  auto cert = GetNearbyShareTestPrivateCertificate(visibility);
  cert.next_salts_for_testing() = std::queue<std::vector<uint8_t>>();
  cert.next_salts_for_testing().push(next_salt_);
  return cert;
}

void FakeNearbyShareCertificateManager::UpdatePrivateCertificateInStorage(
    const NearbySharePrivateCertificate& private_certificate) {}

}  // namespace sharing
}  // namespace nearby
