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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// A fake implementation of NearbyShareCertificateManager, along with a fake
// factory, to be used in tests.
class FakeNearbyShareCertificateManager : public NearbyShareCertificateManager {
 public:
  // Factory that creates FakeNearbyShareCertificateManager instances. Use in
  // NearbyShareCertificateManagerImpl::Factor::SetFactoryForTesting() in unit
  // tests.
  class Factory : public NearbyShareCertificateManagerImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareCertificateManager instances created by
    // CreateInstance().
    std::vector<FakeNearbyShareCertificateManager*>& instances() {
      return instances_;
    }

   private:
    // NearbyShareCertificateManagerImpl::Factory:
    std::unique_ptr<NearbyShareCertificateManager> CreateInstance(
        Context* context,
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        NearbyShareContactManager* contact_manager,
        absl::string_view profile_path,
        nearby::sharing::api::SharingRpcClientFactory* client_factory) override;

    std::vector<FakeNearbyShareCertificateManager*> instances_;
  };

  struct GetDecryptedPublicCertificateCall {
   public:
    GetDecryptedPublicCertificateCall(
        NearbyShareEncryptedMetadataKey encrypted_metadata_key,
        CertDecryptedCallback callback);
    GetDecryptedPublicCertificateCall(
        GetDecryptedPublicCertificateCall&& other);
    GetDecryptedPublicCertificateCall& operator=(
        GetDecryptedPublicCertificateCall&& other);
    GetDecryptedPublicCertificateCall(
        const GetDecryptedPublicCertificateCall&) = delete;
    GetDecryptedPublicCertificateCall& operator=(
        const GetDecryptedPublicCertificateCall&) = delete;
    ~GetDecryptedPublicCertificateCall();

    NearbyShareEncryptedMetadataKey encrypted_metadata_key;
    CertDecryptedCallback callback;
  };

  FakeNearbyShareCertificateManager();
  ~FakeNearbyShareCertificateManager() override;

  // NearbyShareCertificateManager:
  std::vector<nearby::sharing::proto::PublicCertificate>
  GetPrivateCertificatesAsPublicCertificates(
      proto::DeviceVisibility visibility) override;
  void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) override;
  void DownloadPublicCertificates() override;
  void ClearPublicCertificates(std::function<void(bool)> callback) override;
  std::string Dump() const override { return ""; }

  // Make protected methods from base class public in this fake class.
  using NearbyShareCertificateManager::NotifyPrivateCertificatesChanged;
  using NearbyShareCertificateManager::NotifyPublicCertificatesDownloaded;

  void set_next_salt(const std::vector<uint8_t>& salt) { next_salt_ = salt; }

  size_t num_get_private_certificates_as_public_certificates_calls() {
    return num_get_private_certificates_as_public_certificates_calls_;
  }

  size_t num_download_public_certificates_calls() {
    return num_download_public_certificates_calls_;
  }

  size_t num_clear_public_certificates_calls() {
    return num_clear_public_certificates_calls_;
  }

  std::vector<GetDecryptedPublicCertificateCall>&
  get_decrypted_public_certificate_calls() {
    return get_decrypted_public_certificate_calls_;
  }

 private:
  // NearbyShareCertificateManager:
  void OnStart() override;
  void OnStop() override;
  std::optional<NearbySharePrivateCertificate> GetValidPrivateCertificate(
      proto::DeviceVisibility visibility) const override;
  void UpdatePrivateCertificateInStorage(
      const NearbySharePrivateCertificate& private_certificate) override;

  size_t num_get_private_certificates_as_public_certificates_calls_ = 0;
  size_t num_download_public_certificates_calls_ = 0;
  size_t num_clear_public_certificates_calls_ = 0;
  std::vector<GetDecryptedPublicCertificateCall>
      get_decrypted_public_certificate_calls_;
  std::vector<uint8_t> next_salt_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_MANAGER_H_
