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

#include "sharing/certificates/nearby_share_certificate_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/mac_address.h"
#include "internal/test/fake_account_manager.h"
#include "proto/identity/v1/resources.pb.h"
#include "proto/identity/v1/rpcs.pb.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/fake_nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/contacts/fake_nearby_share_contact_manager.h"
#include "sharing/internal/api/fake_nearby_share_client.h"
#include "sharing/internal/api/mock_sharing_platform.h"
#include "sharing/internal/public/pref_names.h"
#include "sharing/internal/test/fake_bluetooth_adapter.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/proto/certificate_rpc.pb.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/scheduling/fake_nearby_share_scheduler.h"
#include "sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby {
namespace sharing {
namespace {
using ::google::nearby::identity::v1::AccountInfo;
using ::google::nearby::identity::v1::Device;
using ::google::nearby::identity::v1::GetAccountInfoResponse;
using ::google::nearby::identity::v1::PublishDeviceRequest;
using ::google::nearby::identity::v1::PublishDeviceResponse;
using ::google::nearby::identity::v1::QuerySharedCredentialsRequest;
using ::google::nearby::identity::v1::QuerySharedCredentialsResponse;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::PublicCertificate;
using ::testing::Not;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAreArray;

const absl::Time t0 = absl::UnixEpoch() + absl::Hours(365 * 50 * 24);

constexpr char kPageTokenPrefix[] = "page_token_";
constexpr char kDeviceId[] = "123456789A";
constexpr char kDefaultDeviceName[] = "Josh's Chromebook";

constexpr absl::string_view kPublicCertificateIds[3] = {"id1", "id2", "id3"};

void CaptureDecryptedPublicCertificateCallback(
    std::optional<NearbyShareDecryptedPublicCertificate>* dest,
    std::optional<NearbyShareDecryptedPublicCertificate> src) {
  *dest = std::move(src);
}

}  // namespace

class NearbyShareCertificateManagerImplTest
    : public ::testing::Test,
      public NearbyShareCertificateManager::Observer {
 public:
  NearbyShareCertificateManagerImplTest() = default;
  ~NearbyShareCertificateManagerImplTest() override = default;

  void SetUp() override {
    ON_CALL(mock_sharing_platform_, GetPreferenceManager)
        .WillByDefault(ReturnRef(preference_manager_));
    ON_CALL(mock_sharing_platform_, GetAccountManager)
        .WillByDefault(ReturnRef(fake_account_manager_));
    // Set time to t0.
    FastForward(t0 - fake_context_.GetClock()->Now());
    preference_manager_.SetString(PrefNames::kDeviceId, kDeviceId);
    local_device_data_manager_ =
        std::make_unique<FakeNearbyShareLocalDeviceDataManager>(
            kDefaultDeviceName);
    contact_manager_ = std::make_unique<FakeNearbyShareContactManager>();

    AccountManager::Account account{
        .display_name = GetNearbyShareTestMetadata().full_name(),
        .picture_url = GetNearbyShareTestMetadata().icon_url(),
        .email = kTestMetadataAccountName,
    };

    fake_account_manager_.SetAccount(account);

    fake_account_manager_.Login("test_client_id", "test_client_secret");

    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(
        &cert_store_factory_);

    // Set default device data.
    local_device_data_manager_->SetDeviceName(
        GetNearbyShareTestMetadata().device_name());
    MacAddress mac_address;
    MacAddress::FromString(kTestUnparsedBluetoothMacAddress, mac_address);
    bluetooth_mac_address_ = mac_address;
    SetMockBluetoothAddress(mac_address);
  }

  void TearDown() override {
    cert_manager_->RemoveObserver(this);
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void Initialize() {
    // Setup Identity API.
    cert_manager_ = NearbyShareCertificateManagerImpl::Factory::Create(
        &fake_context_, mock_sharing_platform_,
        local_device_data_manager_.get(),
        /*profile_path=*/{}, &client_factory_);
    cert_manager_->AddObserver(this);

    cert_store_ = cert_store_factory_.instances().back();
    cert_store_->set_is_sync_mode(true);

    private_cert_exp_scheduler_ =
        scheduler_factory_.pref_name_to_expiration_instance()
            .find(PrefNames::kSchedulerPrivateCertificateExpiration)
            ->second.fake_scheduler;
    public_cert_exp_scheduler_ =
        scheduler_factory_.pref_name_to_expiration_instance()
            .find(PrefNames::kSchedulerPublicCertificateExpiration)
            ->second.fake_scheduler;
    upload_scheduler_ =
        scheduler_factory_.pref_name_to_periodic_instance()
            .find(PrefNames::kSchedulerUploadLocalDeviceCertificates)
            ->second.fake_scheduler;
    download_scheduler_ =
        scheduler_factory_.pref_name_to_periodic_instance()
            .find(PrefNames::kSchedulerDownloadPublicCertificates)
            ->second.fake_scheduler;
    account_info_update_scheduler_ =
        scheduler_factory_.pref_name_to_periodic_instance()
            .find(PrefNames::kSchedulerGetAccountInfo)
            ->second.fake_scheduler;

    PopulatePrivateCertificates();
    PopulatePublicCertificates();
    cert_manager_->StartScheduledTasks();
    EXPECT_TRUE(private_cert_exp_scheduler_->is_running());
    EXPECT_TRUE(public_cert_exp_scheduler_->is_running());
    EXPECT_TRUE(upload_scheduler_->is_running());
    EXPECT_TRUE(download_scheduler_->is_running());
    EXPECT_TRUE(account_info_update_scheduler_->is_running());
  }

  // NearbyShareCertificateManager::Observer:
  void OnPublicCertificatesDownloaded() override {
    ++num_public_certs_downloaded_notifications_;
  }
  void OnPrivateCertificatesChanged() override {
    ++num_private_certs_changed_notifications_;
  }

  FakeNearbyIdentityClient* GetIdentityClient() {
    return client_factory_.identity_instances().back();
  }

 protected:
  enum class DownloadPublicCertificatesResult {
    kSuccess,
    kTimeout,
    kHttpError,
    kStorageError
  };

  absl::Time Now() { return fake_context_.GetClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(absl::Duration delta) {
    fake_context_.fake_clock()->FastForward(delta);
  }

  void Sync() {
    EXPECT_TRUE(fake_context_.last_sequenced_task_runner()->SyncWithTimeout(
        absl::Milliseconds(1000)));
  }

  void SetMockBluetoothAddress(MacAddress bluetooth_mac_address) {
    FakeBluetoothAdapter& bluetooth_adapter =
        *fake_context_.fake_bluetooth_adapter();
    bluetooth_adapter.SetAddress(bluetooth_mac_address);
  }

  void SetBluetoothAdapterIsPresent(bool is_present) {
    if (!is_present) {
      FakeBluetoothAdapter& bluetooth_adapter =
          *fake_context_.fake_bluetooth_adapter();
      bluetooth_adapter.SetAddress(MacAddress{});
    }
  }

  void GetPublicCertificatesCallback(
      bool success, const std::vector<PublicCertificate>& certs) {
    auto& callbacks = cert_store_->get_public_certificates_callbacks();
    auto callback = std::move(callbacks.back());
    callbacks.pop_back();
    auto pub_certs = std::make_unique<std::vector<PublicCertificate>>(
        certs.begin(), certs.end());
    std::move(callback)(success, std::move(pub_certs));
  }

  void VerifyCertificatesUpload(bool expected_force_update_contacts) {
    FakeNearbyIdentityClient* identity_client = GetIdentityClient();
    ASSERT_FALSE(identity_client->publish_device_requests().empty());
    const PublishDeviceRequest& publish_device_request =
        identity_client->publish_device_requests().back();
    EXPECT_EQ(publish_device_request.device().name(),
              absl::StrCat("devices/", kDeviceId));
    EXPECT_EQ(publish_device_request.device()
                  .per_visibility_shared_credentials_size(),
              2);
    EXPECT_EQ(publish_device_request.device()
                  .per_visibility_shared_credentials(0)
                  .shared_credentials_size(),
              kNearbyShareNumPrivateCertificates);
    EXPECT_EQ(publish_device_request.device()
                  .per_visibility_shared_credentials(1)
                  .shared_credentials_size(),
              kNearbyShareNumPrivateCertificates);
    EXPECT_EQ(publish_device_request.device().contact(),
              expected_force_update_contacts
                  ? Device::CONTACT_GOOGLE_CONTACT_LATEST
                  : Device::CONTACT_GOOGLE_CONTACT);
  }

  void InvokePrivateCertificateRefresh(bool expected_success) {
    private_cert_exp_scheduler_->InvokeRequestCallback();
    Sync();
    EXPECT_EQ(1u, private_cert_exp_scheduler_->handled_results().size());
    EXPECT_EQ(expected_success,
              private_cert_exp_scheduler_->handled_results().back());
    EXPECT_EQ(expected_success ? 1u : 0u,
              num_private_certs_changed_notifications_);
    EXPECT_EQ(0u, upload_scheduler_->num_immediate_requests());
    Sync();
    if (expected_success) {
      VerifyCertificatesUpload(/*expected_force_update_contacts=*/false);
    }
  }

  void VerifyPrivateCertificates(
      const nearby::sharing::proto::EncryptedMetadata& expected_metadata) {
    // Expect a full set of certificates for all-contacts and self-share
    std::vector<NearbySharePrivateCertificate> certs =
        cert_store_->GetPrivateCertificates();
    EXPECT_EQ(2 * kNearbyShareNumPrivateCertificates, certs.size());

    absl::Time min_not_before_all_contacts = absl::InfiniteFuture();
    absl::Time min_not_before_self_share = absl::InfiniteFuture();

    absl::Time max_not_after_all_contacts = absl::InfinitePast();
    absl::Time max_not_after_self_share = absl::InfinitePast();

    for (const auto& cert : certs) {
      EXPECT_EQ(cert.not_after() - cert.not_before(),
                kNearbyShareCertificateValidityPeriod);
      switch (cert.visibility()) {
        case DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS:
          min_not_before_all_contacts =
              std::min(min_not_before_all_contacts, cert.not_before());
          max_not_after_all_contacts =
              std::max(max_not_after_all_contacts, cert.not_after());
          break;
        case DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE:
          min_not_before_self_share =
              std::min(min_not_before_self_share, cert.not_before());
          max_not_after_self_share =
              std::max(max_not_after_self_share, cert.not_after());
          break;
        default:
          DCHECK(false);
          break;
      }

      // Verify metadata.
      EXPECT_EQ(expected_metadata.SerializeAsString(),
                cert.unencrypted_metadata().SerializeAsString());
    }

    // Verify contiguous validity periods
    EXPECT_EQ(max_not_after_all_contacts - min_not_before_all_contacts,
              kNearbyShareNumPrivateCertificates *
                  kNearbyShareCertificateValidityPeriod);
    EXPECT_EQ(max_not_after_self_share - min_not_before_self_share,
              kNearbyShareNumPrivateCertificates *
                  kNearbyShareCertificateValidityPeriod);
  }

  void InvokeCertUploadPublishDevice(bool contacts_removed,
                                     bool publish_device_success) {
    FakeNearbyIdentityClient* identity_client = GetIdentityClient();
    std::vector<absl::StatusOr<PublishDeviceResponse>> responses;
    if (contacts_removed) {
      // When contacts are removed, a second publish device call is scheduled.
      PublishDeviceResponse response;
      response.add_contact_updates(
          PublishDeviceResponse::CONTACT_UPDATE_REMOVED);
      responses.push_back(response);
    }
    PublishDeviceResponse response;
    response.add_contact_updates(PublishDeviceResponse::CONTACT_UPDATE_ADDED);
    responses.push_back(response);
    identity_client->SetPublishDeviceResponses(std::move(responses));

    upload_scheduler_->InvokeRequestCallback();
    Sync();
    // If contacts are removed, a second publish device call is scheduled.
    if (contacts_removed) {
      Sync();
      Sync();
    }
    EXPECT_EQ(identity_client->publish_device_requests().size(),
              contacts_removed ? 2 : 1);

    VerifyCertificatesUpload(
        /*expected_force_update_contacts=*/contacts_removed ? false : true);
    EXPECT_EQ(upload_scheduler_->handled_results().size(), 1);
    EXPECT_EQ(upload_scheduler_->handled_results().back(),
              publish_device_success);
  }

  void QuerySharedCredentialsFlow(size_t num_pages,
                                  DownloadPublicCertificatesResult result) {
    size_t prev_num_results = download_scheduler_->handled_results().size();
    cert_store_->SetPublicCertificateIds(kPublicCertificateIds);

    size_t initial_num_notifications =
        num_public_certs_downloaded_notifications_;
    size_t initial_num_public_cert_exp_reschedules =
        public_cert_exp_scheduler_->num_reschedule_calls();

    std::vector<absl::StatusOr<QuerySharedCredentialsResponse>> responses;
    std::string page_token;
    for (size_t page_number = 0; page_number < num_pages; ++page_number) {
      bool last_page = page_number == num_pages - 1;
      if (last_page && result == DownloadPublicCertificatesResult::kHttpError) {
        responses.push_back(absl::InternalError(""));
        break;
      }
      page_token = last_page ? std::string()
                             : absl::StrCat(kPageTokenPrefix, page_number);
      responses.push_back(
          BuildQuerySharedCredentialsResponse(page_number, page_token));
    }

    FakeNearbyIdentityClient* identity_client = GetIdentityClient();
    identity_client->SetQuerySharedCredentialsResponses(responses);
    cert_store_->SetAddPublicCertificatesResult(
        result != DownloadPublicCertificatesResult::kStorageError);
    download_scheduler_->InvokeRequestCallback();
    Sync();

    std::vector<QuerySharedCredentialsRequest> requests =
        identity_client->query_shared_credentials_requests();
    EXPECT_EQ(requests.size(), num_pages);
    EXPECT_EQ(requests.back().name(), absl::StrCat("devices/", kDeviceId));
    ASSERT_EQ(download_scheduler_->handled_results().size(),
              prev_num_results + 1);

    bool success = result == DownloadPublicCertificatesResult::kSuccess;
    EXPECT_EQ(download_scheduler_->handled_results().back(), success);
    EXPECT_EQ(num_public_certs_downloaded_notifications_,
              initial_num_notifications + (success ? 1u : 0u));
    EXPECT_EQ(public_cert_exp_scheduler_->num_reschedule_calls(),
              initial_num_public_cert_exp_reschedules + (success ? 1u : 0u));
  }

  QuerySharedCredentialsResponse BuildQuerySharedCredentialsResponse(
      size_t page_number, absl::string_view page_token) {
    QuerySharedCredentialsResponse response;
    int i = 0;
    for (auto public_certificate : public_certificates_) {
      auto* shared_credential = response.add_shared_credentials();
      shared_credential->set_id(page_number * 100 + i);
      if (i % 2 == 0) {
        shared_credential->set_data_type(
            google::nearby::identity::v1::SharedCredential::
                DATA_TYPE_PUBLIC_CERTIFICATE);
      } else {
        shared_credential->set_data_type(
            google::nearby::identity::v1::SharedCredential::
                DATA_TYPE_SHARED_CREDENTIAL);
      }
      *shared_credential->mutable_data() =
          public_certificate.SerializeAsString();
      i++;
    }
    response.set_next_page_token(page_token);
    return response;
  }

  void CheckStorageAddCertificates(
      const FakeNearbyShareCertificateStorage::AddPublicCertificatesCall&
          add_cert_call) {
    ASSERT_EQ(add_cert_call.public_certificates.size(),
              public_certificates_.size());
    for (size_t i = 0; i < public_certificates_.size(); ++i) {
      EXPECT_EQ(add_cert_call.public_certificates[i].secret_id(),
                public_certificates_[i].secret_id());
    }
  }

  void PopulatePrivateCertificates() {
    private_certificates_.clear();
    const auto& metadata = GetNearbyShareTestMetadata();
    for (auto visibility : {DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
                            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE}) {
      private_certificates_.emplace_back(visibility, t0, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod * 2, metadata);
    }
    for (auto& private_cert : private_certificates_) {
      private_certificate_ids_.push_back(
          std::string(private_cert.id().begin(), private_cert.id().end()));
    }
  }

  void PopulatePublicCertificates() {
    public_certificates_.clear();
    metadata_encryption_keys_.clear();
    auto& metadata1 = GetNearbyShareTestMetadata();
    nearby::sharing::proto::EncryptedMetadata metadata2;
    metadata2.set_device_name("device_name2");
    metadata2.set_full_name("full_name2");
    metadata2.set_icon_url("icon_url2");
    metadata2.set_bluetooth_mac_address("bluetooth_mac_address2");
    for (auto metadata : {metadata1, metadata2}) {
      auto private_cert = NearbySharePrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS, t0, metadata);
      public_certificates_.push_back(*private_cert.ToPublicCertificate());
      metadata_encryption_keys_.push_back(*private_cert.EncryptMetadataKey());
    }
  }

  nearby::sharing::api::MockSharingPlatform mock_sharing_platform_;
  nearby::FakePreferenceManager preference_manager_;
  FakeAccountManager fake_account_manager_;
  FakeContext fake_context_;
  FakeNearbyShareCertificateStorage* cert_store_ = nullptr;
  FakeNearbyShareScheduler* private_cert_exp_scheduler_ = nullptr;
  FakeNearbyShareScheduler* public_cert_exp_scheduler_ = nullptr;
  FakeNearbyShareScheduler* upload_scheduler_ = nullptr;
  FakeNearbyShareScheduler* download_scheduler_ = nullptr;
  FakeNearbyShareScheduler* account_info_update_scheduler_ = nullptr;
  MacAddress bluetooth_mac_address_;
  size_t num_public_certs_downloaded_notifications_ = 0;
  size_t num_private_certs_changed_notifications_ = 0;
  std::vector<NearbySharePrivateCertificate> private_certificates_;
  std::vector<std::string> private_certificate_ids_;
  std::vector<PublicCertificate> public_certificates_;
  std::vector<NearbyShareEncryptedMetadataKey> metadata_encryption_keys_;

  FakeNearbyShareClientFactory client_factory_;
  FakeNearbyShareSchedulerFactory scheduler_factory_;
  FakeNearbyShareCertificateStorage::Factory cert_store_factory_;
  std::unique_ptr<FakeNearbyShareLocalDeviceDataManager>
      local_device_data_manager_;
  std::unique_ptr<FakeNearbyShareContactManager> contact_manager_;
  std::unique_ptr<NearbyShareCertificateManager> cert_manager_;
};

TEST_F(NearbyShareCertificateManagerImplTest,
       EncryptPrivateCertificateMetadataKey) {
  Initialize();
  // No certificates exist for hidden or unspecified visibility.
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_HIDDEN));
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED));

  // No valid certificates exist.
  cert_store_->ReplacePrivateCertificates({});
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));

  // Set up valid all-contacts visibility certificate.
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  // Sanity check that the cert storage is as expected.
  std::vector<NearbySharePrivateCertificate> stored_certs =
      cert_store_->GetPrivateCertificates();
  EXPECT_EQ(stored_certs.at(0).ToCertificateData(),
            private_certificate.ToCertificateData());

  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      cert_manager_->EncryptPrivateCertificateMetadataKey(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
            encrypted_metadata_key->encrypted_key());
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
            encrypted_metadata_key->salt());

  // Verify that storage is updated when salts are consumed during encryption.
  EXPECT_NE(cert_store_->GetPrivateCertificates().at(0).ToCertificateData(),
            private_certificate.ToCertificateData());

  // Set up valid all-contacts visibility certificate. Then test with everyone
  // visibility.
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  std::optional<NearbyShareEncryptedMetadataKey>
      encrypted_metadata_key_everyone =
          cert_manager_->EncryptPrivateCertificateMetadataKey(
              DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
            encrypted_metadata_key_everyone->encrypted_key());
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
            encrypted_metadata_key_everyone->salt());

  // No valid certificates exist.
  FastForward(kNearbyShareCertificateValidityPeriod);
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
}

TEST_F(NearbyShareCertificateManagerImplTest, SignWithPrivateCertificate) {
  Initialize();
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  // Perform sign/verify round trip.
  EXPECT_TRUE(GetNearbyShareTestDecryptedPublicCertificate().VerifySignature(
      GetNearbyShareTestPayloadToSign(),
      *cert_manager_->SignWithPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
          GetNearbyShareTestPayloadToSign())));

  // Set up valid all-contacts visibility certificate. Then test with everyone
  // visibility.
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  // Perform sign/verify round trip.
  EXPECT_TRUE(GetNearbyShareTestDecryptedPublicCertificate().VerifySignature(
      GetNearbyShareTestPayloadToSign(),
      *cert_manager_->SignWithPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
          GetNearbyShareTestPayloadToSign())));

  // No certificates exist for hidden or unspecified visibility.
  EXPECT_FALSE(cert_manager_->SignWithPrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_HIDDEN,
      GetNearbyShareTestPayloadToSign()));

  EXPECT_FALSE(cert_manager_->SignWithPrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED,
      GetNearbyShareTestPayloadToSign()));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       HashAuthenticationTokenWithPrivateCertificate) {
  Initialize();
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  EXPECT_EQ(private_certificate.HashAuthenticationToken(
                GetNearbyShareTestPayloadToSign()),
            cert_manager_->HashAuthenticationTokenWithPrivateCertificate(
                DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
                GetNearbyShareTestPayloadToSign()));

  // Set up valid all-contacts visibility certificate. Then test with everyone
  // visibility.
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  EXPECT_EQ(private_certificate.HashAuthenticationToken(
                GetNearbyShareTestPayloadToSign()),
            cert_manager_->HashAuthenticationTokenWithPrivateCertificate(
                DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
                GetNearbyShareTestPayloadToSign()));

  // No certificates exist for hidden or unspecified visibility.
  EXPECT_FALSE(cert_manager_->HashAuthenticationTokenWithPrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_HIDDEN,
      GetNearbyShareTestPayloadToSign()));

  EXPECT_FALSE(cert_manager_->HashAuthenticationTokenWithPrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED,
      GetNearbyShareTestPayloadToSign()));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateSuccess) {
  Initialize();
  std::optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      [&](std::optional<NearbyShareDecryptedPublicCertificate> cert) {
        CaptureDecryptedPublicCertificateCallback(&decrypted_pub_cert, cert);
      });
  GetPublicCertificatesCallback(true, public_certificates_);

  ASSERT_TRUE(decrypted_pub_cert);
  std::vector<uint8_t> id(public_certificates_[0].secret_id().begin(),
                          public_certificates_[0].secret_id().end());
  EXPECT_EQ(decrypted_pub_cert->id(), id);
  EXPECT_EQ(decrypted_pub_cert->unencrypted_metadata().SerializeAsString(),
            GetNearbyShareTestMetadata().SerializeAsString());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateCertNotFound) {
  Initialize();
  auto private_cert = NearbySharePrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS, t0,
      GetNearbyShareTestMetadata());
  auto metadata_key = private_cert.EncryptMetadataKey();
  ASSERT_TRUE(metadata_key);

  std::optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      *metadata_key,
      [&](std::optional<NearbyShareDecryptedPublicCertificate> cert) {
        CaptureDecryptedPublicCertificateCallback(&decrypted_pub_cert, cert);
      });

  GetPublicCertificatesCallback(true, public_certificates_);

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateGetPublicCertificatesFailure) {
  Initialize();
  std::optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      [&](std::optional<NearbyShareDecryptedPublicCertificate> cert) {
        CaptureDecryptedPublicCertificateCallback(&decrypted_pub_cert, cert);
      });

  GetPublicCertificatesCallback(false, {});

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_F(NearbyShareCertificateManagerImplTest, QuerySharedCredentialsSuccess) {
  Initialize();
  ASSERT_NO_FATAL_FAILURE(QuerySharedCredentialsFlow(
      /*num_pages=*/2, DownloadPublicCertificatesResult::kSuccess));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       QuerySharedCredentialsRPCFailure) {
  Initialize();
  ASSERT_NO_FATAL_FAILURE(QuerySharedCredentialsFlow(
      /*num_pages=*/2, DownloadPublicCertificatesResult::kHttpError));
}

TEST_F(NearbyShareCertificateManagerImplTest, ClearPublicCertificates) {
  Initialize();
  cert_manager_->ClearPublicCertificates([&](bool result) {});
  EXPECT_THAT(cert_store_->clear_public_certificates_callbacks(),
              ::testing::SizeIs(1));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_PublishDevice_NoCertificates_UploadSuccess) {
  Initialize();
  cert_store_->ReplacePrivateCertificates({});

  InvokePrivateCertificateRefresh(/*expected_success=*/true);

  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       ForceUploadPrivateCertificates_PublishDevice_Success) {
  Initialize();
  // All private certificates are valid.
  cert_store_->ReplacePrivateCertificates(private_certificates_);
  cert_manager_->ForceUploadPrivateCertificates();
  Sync();

  EXPECT_EQ(1, upload_scheduler_->num_immediate_requests());
  InvokeCertUploadPublishDevice(/*contacts_removed=*/false,
                                /*publish_device_success=*/true);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       ForceUploadPrivateCertificates_PublishDevice_ContactsRemoved) {
  Initialize();
  // All private certificates are valid.
  cert_store_->ReplacePrivateCertificates(private_certificates_);
  cert_manager_->ForceUploadPrivateCertificates();
  Sync();

  EXPECT_EQ(1, upload_scheduler_->num_immediate_requests());
  InvokeCertUploadPublishDevice(/*contacts_removed=*/true,
                                /*publish_device_success=*/true);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       ForceUploadPrivateCertificates_NotLoggedIn_DoesNotCallPublishDevice) {
  Initialize();
  fake_account_manager_.SetAccount(std::nullopt);
  // All private certificates are valid.
  cert_store_->ReplacePrivateCertificates({});
  cert_manager_->ForceUploadPrivateCertificates();
  Sync();

  EXPECT_EQ(0, upload_scheduler_->num_immediate_requests());
  EXPECT_TRUE(cert_store_->GetPrivateCertificates().empty());
  EXPECT_TRUE(GetIdentityClient()->publish_device_requests().empty());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_OnLocalDeviceMetadataChanged) {
  Initialize();

  // Destroy and recreate private certificates if any metadata fields change.
  for (bool did_device_name_change : {true, false}) {
    for (bool did_full_name_change : {true, false}) {
      for (bool did_icon_change : {true, false}) {
        cert_store_->ReplacePrivateCertificates(private_certificates_);
        local_device_data_manager_->NotifyLocalDeviceDataChanged(
            did_device_name_change, did_full_name_change, did_icon_change);
        Sync();

        std::vector<NearbySharePrivateCertificate> certs =
            cert_store_->GetPrivateCertificates();
        std::vector<std::string> cert_ids;
        for (const auto& cert : certs) {
          cert_ids.push_back(std::string(cert.id().begin(), cert.id().end()));
        }
        if (did_device_name_change || did_full_name_change || did_icon_change) {
          // New certificates should be generated.
          EXPECT_EQ(private_certificate_ids_.size(), cert_ids.size());
          EXPECT_THAT(cert_ids,
                      Not(UnorderedElementsAreArray(private_certificate_ids_)));
        } else {
          EXPECT_THAT(
              private_certificate_ids_,
              UnorderedElementsAreArray(cert_ids.begin(), cert_ids.end()));
        }

        EXPECT_EQ(0, private_cert_exp_scheduler_->num_immediate_requests());
      }
    }
  }
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_PublishDevice_OnVendorIdChanged) {
  Initialize();
  cert_store_->ReplacePrivateCertificates(private_certificates_);

  cert_manager_->SetVendorId(12345);

  Sync();
  std::vector<NearbySharePrivateCertificate> certs =
      cert_store_->GetPrivateCertificates();
  std::vector<std::string> cert_ids;
  for (const auto& cert : certs) {
    cert_ids.push_back(std::string(cert.id().begin(), cert.id().end()));
  }
  // New certificates should be generated.
  EXPECT_EQ(private_certificate_ids_.size(), cert_ids.size());
  EXPECT_THAT(cert_ids,
              Not(UnorderedElementsAreArray(private_certificate_ids_)));

  auto metadata = GetNearbyShareTestMetadata();
  metadata.set_vendor_id(12345);
  VerifyPrivateCertificates(/*expected_metadata=*/metadata);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       SetVendorId_WhenNoPrivateCertificates) {
  Initialize();
  cert_store_->ReplacePrivateCertificates({});

  cert_manager_->SetVendorId(12345);

  Sync();
  std::vector<NearbySharePrivateCertificate> certs =
      cert_store_->GetPrivateCertificates();
  std::vector<std::string> cert_ids;
  for (const auto& cert : certs) {
    cert_ids.push_back(std::string(cert.id().begin(), cert.id().end()));
  }
  // New certificates should be generated.
  EXPECT_EQ(private_certificate_ids_.size(), cert_ids.size());
  EXPECT_THAT(cert_ids,
              Not(UnorderedElementsAreArray(private_certificate_ids_)));

  auto metadata = GetNearbyShareTestMetadata();
  metadata.set_vendor_id(12345);
  VerifyPrivateCertificates(/*expected_metadata=*/metadata);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_PublishDevice_ExpiredCertificate) {
  Initialize();
  // First certificates are expired;
  FastForward(kNearbyShareCertificateValidityPeriod * 1.5);
  cert_store_->ReplacePrivateCertificates(private_certificates_);

  InvokePrivateCertificateRefresh(/*expected_success=*/true);

  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_BluetoothAdapterNotPresent) {
  Initialize();
  cert_store_->ReplacePrivateCertificates({});

  SetBluetoothAdapterIsPresent(false);

  // Bluetooth MAC address is optional, so the refresh should still succeed.
  InvokePrivateCertificateRefresh(/*expected_success=*/true);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RemoveExpiredPublicCertificates_Success) {
  Initialize();
  // The public certificate expiration scheduler notifies the certificate
  // manager that a public certificate has expired.
  EXPECT_EQ(cert_store_->remove_expired_public_certificates_calls().size(), 0u);
  EXPECT_EQ(public_cert_exp_scheduler_->handled_results().size(), 0u);
  cert_store_->SetRemoveExpiredPublicCertificatesResult(true);
  public_cert_exp_scheduler_->InvokeRequestCallback();
  Sync();
  EXPECT_EQ(cert_store_->remove_expired_public_certificates_calls().size(), 1u);
  EXPECT_EQ(cert_store_->remove_expired_public_certificates_calls().back().now,
            t0);
  EXPECT_EQ(public_cert_exp_scheduler_->handled_results().size(), 1u);
  EXPECT_TRUE(public_cert_exp_scheduler_->handled_results().back());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RemoveExpiredPublicCertificates_Failure) {
  Initialize();
  // The public certificate expiration scheduler notifies the certificate
  // manager that a public certificate has expired.
  EXPECT_EQ(cert_store_->remove_expired_public_certificates_calls().size(), 0u);
  cert_store_->SetRemoveExpiredPublicCertificatesResult(false);
  EXPECT_EQ(public_cert_exp_scheduler_->handled_results().size(), 0u);

  public_cert_exp_scheduler_->InvokeRequestCallback();
  Sync();
  EXPECT_EQ(cert_store_->remove_expired_public_certificates_calls().size(), 1u);
  EXPECT_EQ(cert_store_->remove_expired_public_certificates_calls().back().now,
            t0);

  EXPECT_EQ(public_cert_exp_scheduler_->handled_results().size(), 1u);
  EXPECT_FALSE(public_cert_exp_scheduler_->handled_results().back());
}

TEST_F(
    NearbyShareCertificateManagerImplTest,
    ForceUploadPrivateCertificates_NoLogIn_DisablesPrivateCertExpirationTimer) {
  Initialize();
  fake_account_manager_.SetAccount(std::nullopt);
  // All private certificates are valid.
  cert_store_->ReplacePrivateCertificates({});
  cert_manager_->ForceUploadPrivateCertificates();
  Sync();

  EXPECT_EQ(0, upload_scheduler_->num_immediate_requests());
  absl::Time next_schedule_time =
      scheduler_factory_.pref_name_to_expiration_instance()
          .find(PrefNames::kSchedulerPrivateCertificateExpiration)
          ->second.expiration_time_functor();

  // Next expiration time is set to InfiniteFuture to disable the timer.
  EXPECT_EQ(next_schedule_time, absl::InfiniteFuture());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       UploadCertificates_PublishDevice_NoPrivateCertificates) {
  Initialize();
  cert_store_->ReplacePrivateCertificates({});

  upload_scheduler_->InvokeRequestCallback();
  Sync();
  EXPECT_TRUE(GetIdentityClient()->publish_device_requests().empty());
  EXPECT_EQ(upload_scheduler_->handled_results().size(), 1);
  EXPECT_EQ(upload_scheduler_->handled_results().back(), false);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificates_Success) {
  Initialize();

  cert_manager_->DownloadPublicCertificates();

  EXPECT_EQ(download_scheduler_->num_immediate_requests(), 1);
}

TEST_F(NearbyShareCertificateManagerImplTest, StopScheduledTasks) {
  Initialize();
  cert_manager_->StopScheduledTasks();
  EXPECT_FALSE(private_cert_exp_scheduler_->is_running());
  EXPECT_FALSE(public_cert_exp_scheduler_->is_running());
  EXPECT_FALSE(upload_scheduler_->is_running());
  EXPECT_FALSE(download_scheduler_->is_running());
  EXPECT_FALSE(account_info_update_scheduler_->is_running());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       UpdateAccountInfo_TitanumEnabled) {
  Initialize();
  FakeNearbyIdentityClient* identity_client = GetIdentityClient();
  GetAccountInfoResponse response;
  response.mutable_account_info()->mutable_capabilities()->Add(
      AccountInfo::CAPABILITY_TITANIUM);
  identity_client->SetGetAccountInfoResponse(response);

  account_info_update_scheduler_->InvokeRequestCallback();
  Sync();

  EXPECT_FALSE(GetIdentityClient()->get_account_info_requests().empty());
  EXPECT_TRUE(preference_manager_.GetBoolean(
      PrefNames::kAdvancedProtectionEnabled, /*default_value=*/false));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       UpdateAccountInfo_TitanumDisabled) {
  Initialize();
  preference_manager_.SetBoolean(PrefNames::kAdvancedProtectionEnabled, true);
  FakeNearbyIdentityClient* identity_client = GetIdentityClient();
  GetAccountInfoResponse response;
  identity_client->SetGetAccountInfoResponse(response);

  account_info_update_scheduler_->InvokeRequestCallback();
  Sync();

  EXPECT_FALSE(GetIdentityClient()->get_account_info_requests().empty());
  EXPECT_FALSE(preference_manager_.GetBoolean(
      PrefNames::kAdvancedProtectionEnabled, /*default_value=*/false));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       UpdateAccountInfo_TitanumUnspecified) {
  Initialize();
  preference_manager_.SetBoolean(PrefNames::kAdvancedProtectionEnabled, true);
  FakeNearbyIdentityClient* identity_client = GetIdentityClient();
  GetAccountInfoResponse response;
  response.mutable_account_info()->mutable_capabilities()->Add(
      AccountInfo::CAPABILITY_UNSPECIFIED);
  identity_client->SetGetAccountInfoResponse(response);

  account_info_update_scheduler_->InvokeRequestCallback();
  Sync();

  EXPECT_FALSE(GetIdentityClient()->get_account_info_requests().empty());
  EXPECT_FALSE(preference_manager_.GetBoolean(
      PrefNames::kAdvancedProtectionEnabled, /*default_value=*/false));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       UpdateAccountInfo_RpcFailed) {
  Initialize();
  preference_manager_.SetBoolean(PrefNames::kAdvancedProtectionEnabled, true);

  account_info_update_scheduler_->InvokeRequestCallback();
  Sync();

  // Identity client by default return Status::NotFound.
  EXPECT_FALSE(GetIdentityClient()->get_account_info_requests().empty());
  EXPECT_TRUE(preference_manager_.GetBoolean(
      PrefNames::kAdvancedProtectionEnabled, /*default_value=*/false));
}

}  // namespace sharing
}  // namespace nearby
