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
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/fake_nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/contacts/fake_nearby_share_contact_manager.h"
#include "sharing/internal/api/fake_nearby_share_client.h"
#include "sharing/internal/api/mock_sharing_platform.h"
#include "sharing/internal/public/logging.h"
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
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::PublicCertificate;
using ::testing::ReturnRef;

const absl::Time t0 = absl::UnixEpoch() + absl::Hours(365 * 50 * 24);

constexpr char kPageTokenPrefix[] = "page_token_";
constexpr char kSecretIdPrefix[] = "secret_id_";
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

    local_device_data_manager_ =
        std::make_unique<FakeNearbyShareLocalDeviceDataManager>(
            kDefaultDeviceName);
    local_device_data_manager_->set_is_sync_mode(true);
    local_device_data_manager_->SetId(kDeviceId);

    contact_manager_ = std::make_unique<FakeNearbyShareContactManager>();

    AccountManager::Account account{
        .email = kTestMetadataAccountName,
    };

    fake_account_manager_.SetAccount(account);

    fake_account_manager_.Login([](AccountManager::Account account) {},
                                []() {});

    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(
        &cert_store_factory_);

    // Set default device data.
    local_device_data_manager_->SetDeviceName(
        GetNearbyShareTestMetadata().device_name());
    local_device_data_manager_->SetFullName(
        GetNearbyShareTestMetadata().full_name());
    local_device_data_manager_->SetIconUrl(
        GetNearbyShareTestMetadata().icon_url());
    SetBluetoothMacAddress(kTestUnparsedBluetoothMacAddress);
    SetMockBluetoothAddress(kTestUnparsedBluetoothMacAddress);

    cert_manager_ = NearbyShareCertificateManagerImpl::Factory::Create(
        &fake_context_, mock_sharing_platform_,
        local_device_data_manager_.get(), contact_manager_.get(), std::string(),
        &client_factory_);
    cert_manager_->AddObserver(this);

    cert_store_ = cert_store_factory_.instances().back();
    cert_store_->set_is_sync_mode(true);

    private_cert_exp_scheduler_ =
        scheduler_factory_.pref_name_to_expiration_instance()
            .find(
                prefs::kNearbySharingSchedulerPrivateCertificateExpirationName)
            ->second.fake_scheduler;
    public_cert_exp_scheduler_ =
        scheduler_factory_.pref_name_to_expiration_instance()
            .find(prefs::kNearbySharingSchedulerPublicCertificateExpirationName)
            ->second.fake_scheduler;
    upload_scheduler_ =
        scheduler_factory_.pref_name_to_on_demand_instance()
            .find(
                prefs::
                    kNearbySharingSchedulerUploadLocalDeviceCertificatesName)  // NOLINT
            ->second.fake_scheduler;
    download_scheduler_ =
        scheduler_factory_.pref_name_to_periodic_instance()
            .find(prefs::kNearbySharingSchedulerDownloadPublicCertificatesName)
            ->second.fake_scheduler;

    PopulatePrivateCertificates();
    PopulatePublicCertificates();
    cert_manager_->Start();
  }

  void TearDown() override {
    cert_manager_->RemoveObserver(this);
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void SetBluetoothMacAddress(absl::string_view bluetooth_mac_address) {
    bluetooth_mac_address_ = bluetooth_mac_address;
  }

  // NearbyShareCertificateManager::Observer:
  void OnPublicCertificatesDownloaded() override {
    ++num_public_certs_downloaded_notifications_;
  }
  void OnPrivateCertificatesChanged() override {
    ++num_private_certs_changed_notifications_;
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
    EXPECT_TRUE(FakeTaskRunner::WaitForRunningTasksWithTimeout(
        absl::Milliseconds(1000)));
  }

  void SetMockBluetoothAddress(absl::string_view bluetooth_mac_address) {
    FakeBluetoothAdapter& bluetooth_adapter =
        dynamic_cast<FakeBluetoothAdapter&>(
            fake_context_.GetBluetoothAdapter());
    bluetooth_adapter.SetAddress(bluetooth_mac_address);
  }

  void SetBluetoothAdapterIsPresent(bool is_present) {
    if (!is_present) {
      FakeBluetoothAdapter& bluetooth_adapter =
          dynamic_cast<FakeBluetoothAdapter&>(
              fake_context_.GetBluetoothAdapter());
      bluetooth_adapter.SetAddress("");
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

  void HandlePrivateCertificateRefresh(bool expect_private_cert_refresh,
                                       bool expected_success) {
    if (expect_private_cert_refresh) {
      private_cert_exp_scheduler_->InvokeRequestCallback();
    }
    Sync();
    EXPECT_EQ(expect_private_cert_refresh ? 1u : 0u,
              private_cert_exp_scheduler_->handled_results().size());
    if (expect_private_cert_refresh) {
      EXPECT_EQ(expected_success,
                private_cert_exp_scheduler_->handled_results().back());
    }
    EXPECT_EQ(expect_private_cert_refresh && expected_success ? 1u : 0u,
              num_private_certs_changed_notifications_);
    EXPECT_EQ(expect_private_cert_refresh && expected_success ? 1u : 0u,
              upload_scheduler_->num_immediate_requests());
  }

  void VerifyPrivateCertificates(
      const nearby::sharing::proto::EncryptedMetadata& expected_metadata) {
    // Expect a full set of certificates for all-contacts, selected-contacts,
    // and self-share
    std::vector<NearbySharePrivateCertificate> certs =
        *cert_store_->GetPrivateCertificates();
    EXPECT_EQ(3 * kNearbyShareNumPrivateCertificates, certs.size());

    absl::Time min_not_before_all_contacts = absl::InfiniteFuture();
    absl::Time min_not_before_selected_contacts = absl::InfiniteFuture();
    absl::Time min_not_before_self_share = absl::InfiniteFuture();

    absl::Time max_not_after_all_contacts = absl::InfinitePast();
    absl::Time max_not_after_selected_contacts = absl::InfinitePast();
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
        case DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS:
          min_not_before_selected_contacts =
              std::min(min_not_before_selected_contacts, cert.not_before());
          max_not_after_selected_contacts =
              std::max(max_not_after_selected_contacts, cert.not_after());
          break;
        case DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE:
          min_not_before_self_share =
              std::min(min_not_before_self_share, cert.not_before());
          max_not_after_self_share =
              std::max(max_not_after_self_share, cert.not_after());
          break;
        default:
          NL_DCHECK(false);
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
    EXPECT_EQ(
        max_not_after_selected_contacts - min_not_before_selected_contacts,
        kNearbyShareNumPrivateCertificates *
            kNearbyShareCertificateValidityPeriod);
  }

  void RunUpload(bool success) {
    size_t initial_num_upload_calls =
        local_device_data_manager_->upload_certificates_calls().size();
    local_device_data_manager_->SetUploadCertificatesResult(success);
    size_t initial_num_handled_results =
        upload_scheduler_->handled_results().size();

    upload_scheduler_->InvokeRequestCallback();
    Sync();
    EXPECT_EQ(local_device_data_manager_->upload_certificates_calls().size(),
              initial_num_upload_calls + 1);

    EXPECT_EQ(local_device_data_manager_->upload_certificates_calls()
                  .back()
                  .certificates.size(),
              3 * kNearbyShareNumPrivateCertificates);

    EXPECT_EQ(upload_scheduler_->handled_results().size(),
              initial_num_handled_results + 1);
    EXPECT_EQ(upload_scheduler_->handled_results().back(), success);
  }

  // Test downloading public certificates with or without errors. The RPC is
  // paginated, and |num_pages| will be simulated. Any failures, as indicated by
  // |result|, will be simulated on the last page.
  void DownloadPublicCertificatesFlow(size_t num_pages,
                                      DownloadPublicCertificatesResult result) {
    size_t prev_num_results = download_scheduler_->handled_results().size();
    cert_store_->SetPublicCertificateIds(kPublicCertificateIds);

    size_t initial_num_notifications =
        num_public_certs_downloaded_notifications_;
    size_t initial_num_public_cert_exp_reschedules =
        public_cert_exp_scheduler_->num_reschedule_calls();

    // Build RPC responses.
    std::vector<absl::StatusOr<proto::ListPublicCertificatesResponse>>
        responses;
    std::string page_token;
    for (size_t page_number = 0; page_number < num_pages; ++page_number) {
      bool last_page = page_number == num_pages - 1;
      if (last_page && result == DownloadPublicCertificatesResult::kHttpError) {
        responses.push_back(absl::InternalError(""));
        break;
      }
      page_token = last_page ? std::string()
                             : absl::StrCat(kPageTokenPrefix, page_number);
      responses.push_back(BuildRpcResponse(page_number, page_token));
    }

    client_factory_.instances().back()->SetListPublicCertificatesResponses(
        responses);
    cert_store_->SetAddPublicCertificatesResult(
        result != DownloadPublicCertificatesResult::kStorageError);
    download_scheduler_->InvokeRequestCallback();
    Sync();

    CheckRpcRequest(num_pages);
    ASSERT_EQ(download_scheduler_->handled_results().size(),
              prev_num_results + 1);

    bool success = result == DownloadPublicCertificatesResult::kSuccess;
    EXPECT_EQ(download_scheduler_->handled_results().back(), success);
    EXPECT_EQ(num_public_certs_downloaded_notifications_,
              initial_num_notifications + (success ? 1u : 0u));
    EXPECT_EQ(public_cert_exp_scheduler_->num_reschedule_calls(),
              initial_num_public_cert_exp_reschedules + (success ? 1u : 0u));
  }

  void CheckRpcRequest(int num_pages) {
    std::vector<proto::ListPublicCertificatesRequest> requests =
        client_factory_.instances().back()->list_public_certificates_requests();
    EXPECT_EQ(requests.size(), num_pages);
  }

  nearby::sharing::proto::ListPublicCertificatesResponse BuildRpcResponse(
      size_t page_number, absl::string_view page_token) {
    nearby::sharing::proto::ListPublicCertificatesResponse response;
    for (size_t i = 0; i < public_certificates_.size(); ++i) {
      public_certificates_[i].set_secret_id(
          absl::StrCat(kSecretIdPrefix, page_number, "_", i));
      response.add_public_certificates();
      *response.mutable_public_certificates(i) = public_certificates_[i];
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
    for (auto visibility :
         {DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS,
          DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS,
          DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE}) {
      private_certificates_.emplace_back(visibility, t0, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod * 2, metadata);
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
  std::string bluetooth_mac_address_ = kTestUnparsedBluetoothMacAddress;
  size_t num_public_certs_downloaded_notifications_ = 0;
  size_t num_private_certs_changed_notifications_ = 0;
  std::vector<NearbySharePrivateCertificate> private_certificates_;
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
  // No valid certificates exist.
  cert_store_->ReplacePrivateCertificates({});
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS));

  // Set up valid all-contacts visibility certificate.
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store_->ReplacePrivateCertificates({private_certificate});
  FastForward(GetNearbyShareTestNotBefore() +
              kNearbyShareCertificateValidityPeriod * 0.5 - Now());

  // Sanity check that the cert storage is as expected.
  std::optional<std::vector<NearbySharePrivateCertificate>> stored_certs =
      cert_store_->GetPrivateCertificates();
  EXPECT_EQ(stored_certs->at(0).ToCertificateData(),
            private_certificate.ToCertificateData());

  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      cert_manager_->EncryptPrivateCertificateMetadataKey(
          DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
            encrypted_metadata_key->encrypted_key());
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
            encrypted_metadata_key->salt());
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS));

  // Verify that storage is updated when salts are consumed during encryption.
  EXPECT_NE(cert_store_->GetPrivateCertificates()->at(0).ToCertificateData(),
            private_certificate.ToCertificateData());

  // No valid certificates exist.
  FastForward(kNearbyShareCertificateValidityPeriod);
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS));
  EXPECT_FALSE(cert_manager_->EncryptPrivateCertificateMetadataKey(
      DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS));
}

TEST_F(NearbyShareCertificateManagerImplTest, SignWithPrivateCertificate) {
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

  // No selected-contact visibility certificate in storage.
  EXPECT_FALSE(cert_manager_->SignWithPrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS,
      GetNearbyShareTestPayloadToSign()));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       HashAuthenticationTokenWithPrivateCertificate) {
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

  // No selected-contact visibility certificate in storage.
  EXPECT_FALSE(cert_manager_->HashAuthenticationTokenWithPrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS,
      GetNearbyShareTestPayloadToSign()));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateSuccess) {
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
  std::optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_manager_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      [&](std::optional<NearbyShareDecryptedPublicCertificate> cert) {
        CaptureDecryptedPublicCertificateCallback(&decrypted_pub_cert, cert);
      });

  GetPublicCertificatesCallback(false, {});

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesSuccess) {
  ASSERT_NO_FATAL_FAILURE(DownloadPublicCertificatesFlow(
      /*num_pages=*/2, DownloadPublicCertificatesResult::kSuccess));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesRPCFailure) {
  ASSERT_NO_FATAL_FAILURE(DownloadPublicCertificatesFlow(
      /*num_pages=*/2, DownloadPublicCertificatesResult::kHttpError));
}

TEST_F(NearbyShareCertificateManagerImplTest, ClearPublicCertificates) {
  cert_manager_->ClearPublicCertificates([&](bool result) {});
  EXPECT_THAT(cert_store_->clear_public_certificates_callbacks(),
              ::testing::SizeIs(1));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesStoreFailure) {
  ASSERT_NO_FATAL_FAILURE(DownloadPublicCertificatesFlow(
      /*num_pages=*/2, DownloadPublicCertificatesResult::kStorageError));
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_ValidCertificates) {
  cert_store_->ReplacePrivateCertificates(private_certificates_);

  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/false,
                                  /*expected_success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_NoCertificates_UploadSuccess) {
  cert_store_->ReplacePrivateCertificates({});

  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_NoCertificates_UploadFailure) {
  cert_store_->ReplacePrivateCertificates({});

  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/false);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RevokePrivateCertificates_OnContactsUploaded) {
  // Destroy and recreate private certificates if contact data has changed since
  // the last successful upload.
  cert_manager_->Stop();
  size_t num_expected_calls = 0;
  for (bool did_contacts_change_since_last_upload : {true, false}) {
    cert_store_->ReplacePrivateCertificates(private_certificates_);
    contact_manager_->NotifyContactsUploaded(
        did_contacts_change_since_last_upload);

    Sync();
    std::vector<NearbySharePrivateCertificate> certs =
        *cert_store_->GetPrivateCertificates();

    if (did_contacts_change_since_last_upload) {
      ++num_expected_calls;
      EXPECT_TRUE(certs.empty());
    } else {
      EXPECT_EQ(certs.size(), 9u);
    }

    EXPECT_EQ(num_expected_calls,
              private_cert_exp_scheduler_->num_immediate_requests());
  }
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_OnLocalDeviceMetadataChanged) {
  cert_manager_->Start();

  // Destroy and recreate private certificates if any metadata fields change.
  size_t num_expected_calls = 0;
  for (bool did_device_name_change : {true, false}) {
    for (bool did_full_name_change : {true, false}) {
      for (bool did_icon_change : {true, false}) {
        local_device_data_manager_->NotifyLocalDeviceDataChanged(
            did_device_name_change, did_full_name_change, did_icon_change);
        Sync();

        if (did_device_name_change || did_full_name_change || did_icon_change) {
          ++num_expected_calls;
          EXPECT_TRUE(cert_store_->GetPrivateCertificates()->empty());
        }

        EXPECT_EQ(num_expected_calls,
                  private_cert_exp_scheduler_->num_immediate_requests());
      }
    }
  }
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_ExpiredCertificate) {
  // First certificates are expired;
  FastForward(kNearbyShareCertificateValidityPeriod * 1.5);
  cert_store_->ReplacePrivateCertificates(private_certificates_);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);
  VerifyPrivateCertificates(/*expected_metadata=*/GetNearbyShareTestMetadata());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_InvalidDeviceName) {
  cert_store_->ReplacePrivateCertificates({});

  // Device name is missing in local device data manager.
  local_device_data_manager_->SetDeviceName(std::string());

  cert_manager_->Start();

  // Expect failure because a device name is required.
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/false);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_BluetoothAdapterNotPresent) {
  cert_store_->ReplacePrivateCertificates({});

  SetBluetoothAdapterIsPresent(false);

  cert_manager_->Start();

  // Expect failure because a Bluetooth MAC address is required.
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/false);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RefreshPrivateCertificates_MissingFullNameAndIconUrl) {
  cert_store_->ReplacePrivateCertificates({});

  // Full name and icon URL are missing in the local device data manager.
  local_device_data_manager_->SetFullName(std::nullopt);
  local_device_data_manager_->SetIconUrl(std::nullopt);

  cert_manager_->Start();
  HandlePrivateCertificateRefresh(/*expect_private_cert_refresh=*/true,
                                  /*expected_success=*/true);
  RunUpload(/*success=*/true);

  // The full name and icon URL are not set.
  nearby::sharing::proto::EncryptedMetadata metadata =
      GetNearbyShareTestMetadata();
  metadata.clear_full_name();
  metadata.clear_icon_url();

  VerifyPrivateCertificates(/*expected_metadata=*/metadata);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       RemoveExpiredPublicCertificates_Success) {
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

}  // namespace sharing
}  // namespace nearby
