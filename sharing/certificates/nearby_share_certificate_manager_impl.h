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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/task_runner.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

class NearbyShareScheduler;

// An implementation of the NearbyShareCertificateManager that handles
//   1) creating, storing, and uploading local device certificates, as well as
//      removing expired/revoked local device certificates;
//   2) downloading, storing, and decrypting public certificates from trusted
//      contacts, as well as removing expired public certificates.
//
// This implementation destroys and recreates all private certificates if there
// are any changes to the user's contact list or allowlist, or if there are any
// changes to the local device data, such as the device name.
class NearbyShareCertificateManagerImpl
    : public NearbyShareCertificateManager,
      public NearbyShareLocalDeviceDataManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareCertificateManager> Create(
        Context* context,
        nearby::sharing::api::SharingPlatform& sharing_platform,
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        const FilePath& profile_path,
        nearby::sharing::api::SharingRpcClientFactory* client_factory);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareCertificateManager> CreateInstance(
        Context* context,
        NearbyShareLocalDeviceDataManager* local_device_data_manager,
        const FilePath& profile_path,
        nearby::sharing::api::SharingRpcClientFactory* client_factory) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareCertificateManagerImpl() override;

  void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) override;
  void DownloadPublicCertificates() override;
  void ForceUploadPrivateCertificates() override;
  void ClearPublicCertificates(std::function<void(bool)> callback) override;
  void SetVendorId(int32_t vendor_id) override;
  std::string Dump() const override;

 private:
  // Class for maintaining a single instance of public certificate download
  // request.  It is responsible for downloading all available pages and making
  // the results or error available.
  class CertificateDownloadContext {
   public:
    CertificateDownloadContext(
        nearby::sharing::api::IdentityRpcClient* nearby_identity_client,
        std::string device_id,
        absl::AnyInvocable<void() &&> download_failure_callback,
        absl::AnyInvocable<
            void(const std::vector<nearby::sharing::proto::PublicCertificate>&
                     certificates) &&>
            download_success_callback)
        : nearby_identity_client_(nearby_identity_client),
          device_id_(std::move(device_id)),
          download_failure_callback_(std::move(download_failure_callback)),
          download_success_callback_(std::move(download_success_callback)) {}

    // Fetches the next page of certificates by calling Identity API
    // QuerySharedCredentials.
    // If |next_page_token_| is empty, it fetches the first page.
    // On successful download, if  page token in the response is empty, the
    // |download_success_callback_| is invoked with all downloaded certificates.
    void QuerySharedCredentialsFetchNextPage();

   private:
    nearby::sharing::api::IdentityRpcClient* const nearby_identity_client_;
    std::string device_id_;
    std::optional<std::string> next_page_token_;
    int page_number_ = 1;
    std::vector<nearby::sharing::proto::PublicCertificate> certificates_;
    absl::AnyInvocable<void() &&> download_failure_callback_;
    absl::AnyInvocable<
        void(const std::vector<nearby::sharing::proto::PublicCertificate>&
                 certificates) &&>
        download_success_callback_;
  };

  NearbyShareCertificateManagerImpl(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      AccountManager& account_manager,
      std::unique_ptr<nearby::sharing::api::PublicCertificateDatabase>
          public_certificate_database,
      NearbyShareLocalDeviceDataManager* local_device_data_manager,
       nearby::sharing::api::SharingRpcClientFactory* client_factory);

  // NearbyShareCertificateManager:
  void OnStart() override;
  void OnStop() override;
  std::optional<NearbySharePrivateCertificate> GetValidPrivateCertificate(
      proto::DeviceVisibility visibility) const override;
  void UpdatePrivateCertificateInStorage(
      const NearbySharePrivateCertificate& private_certificate) override;

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_change) override;

  // Used by the private certificate expiration scheduler to determine the next
  // private certificate expiration time. Returns InfinitePast() if
  // certificates are missing.  Returns InfiniteFuture() if the user is not
  // logged in.
  absl::Time NextPrivateCertificateExpirationTime();

  // Used by the public certificate expiration scheduler to determine the next
  // public certificate expiration time. Returns InfiniteFuture() if no public
  // certificates are present, and no expiration event is scheduled.
  absl::Time NextPublicCertificateExpirationTime();

  // Clears all existing private certificates and regenerates new ones, then
  // uploads them to the server, without triggering contacts update.
  void RegeneratePrivateCertificates();

  // Certificate operations that run on the executor.
  // Returns true if the operation was successful.
  bool RefreshPrivateCertificatesInExecutor(bool force_upload);
  bool UploadDeviceCertificatesInExecutor(
      const std::vector<NearbySharePrivateCertificate>& private_certs,
      bool force_update_contacts);
  bool DownloadPublicCertificatesInExecutor();
  bool RemoveExpiredPublicCertificatesInExecutor();

  // Updates the public certificates in the certificate storage. Returns true if
  // the certificates were successfully updated.
  bool UpdatePublicCertificates(
      const std::vector<nearby::sharing::proto::PublicCertificate>&
          certificates);

  void AddCertifactesToPublishDeviceRequest(
      const std::vector<NearbySharePrivateCertificate>& private_certs,
      google::nearby::identity::v1::PublishDeviceRequest& request);

  // Returns the device id use to identify the local device in BE.
  std::string GetId();


  Context* const context_;
  AccountManager& account_manager_;
  NearbyShareLocalDeviceDataManager* const local_device_data_manager_;
  nearby::sharing::api::PreferenceManager& preference_manager_;
  int32_t vendor_id_ = 0;  // Defaults to GOOGLE.
  std::unique_ptr< nearby::sharing::api::SharingRpcClient> nearby_client_;
  std::unique_ptr<nearby::sharing::api::IdentityRpcClient>
      nearby_identity_client_;

  std::shared_ptr<NearbyShareCertificateStorage> certificate_storage_;
  std::unique_ptr<NearbyShareScheduler>
      private_certificate_expiration_scheduler_;
  std::unique_ptr<NearbyShareScheduler>
      public_certificate_expiration_scheduler_;
  std::unique_ptr<NearbyShareScheduler> force_contacts_update_scheduler_;
  std::unique_ptr<NearbyShareScheduler> download_public_certificates_scheduler_;

  std::unique_ptr<TaskRunner> executor_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_IMPL_H_
