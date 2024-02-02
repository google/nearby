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

#include "sharing/certificates/nearby_share_certificate_storage_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/api/mock_public_certificate_db.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/internal/test/fake_public_certificate_db.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::sharing::api::MockPublicCertificateDb;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::PublicCertificate;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::StrictMock;

// NOTE: Make sure secret ID alphabetical ordering does not match the 1,2,3,...
// ordering to test sorting expiration times.
constexpr char kSecretId1[] = "b_secretid1";
constexpr char kSecretKey1[] = "secretkey1";
constexpr char kPublicKey1[] = "publickey1";
constexpr int64_t kStartSeconds1 = 0;
constexpr int32_t kStartNanos1 = 10;
constexpr int64_t kEndSeconds1 = 100;
constexpr int32_t kEndNanos1 = 30;
constexpr bool kForSelectedContacts1 = false;
constexpr char kMetadataEncryptionKey1[] = "metadataencryptionkey1";
constexpr char kEncryptedMetadataBytes1[] = "encryptedmetadatabytes1";
constexpr char kMetadataEncryptionKeyTag1[] = "metadataencryptionkeytag1";
constexpr char kSecretId2[] = "c_secretid2";
constexpr char kSecretKey2[] = "secretkey2";
constexpr char kPublicKey2[] = "publickey2";
constexpr int64_t kStartSeconds2 = 0;
constexpr int32_t kStartNanos2 = 20;
constexpr int64_t kEndSeconds2 = 200;
constexpr int32_t kEndNanos2 = 30;
constexpr bool kForSelectedContacts2 = false;
constexpr char kMetadataEncryptionKey2[] = "metadataencryptionkey2";
constexpr char kEncryptedMetadataBytes2[] = "encryptedmetadatabytes2";
constexpr char kMetadataEncryptionKeyTag2[] = "metadataencryptionkeytag2";
constexpr char kSecretId3[] = "a_secretid3";
constexpr char kSecretKey3[] = "secretkey3";
constexpr char kPublicKey3[] = "publickey3";
constexpr int64_t kStartSeconds3 = 0;
constexpr int32_t kStartNanos3 = 30;
constexpr int64_t kEndSeconds3 = 300;
constexpr int32_t kEndNanos3 = 30;
constexpr bool kForSelectedContacts3 = false;
constexpr char kMetadataEncryptionKey3[] = "metadataencryptionkey3";
constexpr char kEncryptedMetadataBytes3[] = "encryptedmetadatabytes3";
constexpr char kMetadataEncryptionKeyTag3[] = "metadataencryptionkeytag3";
constexpr char kSecretId4[] = "d_secretid4";
constexpr char kSecretKey4[] = "secretkey4";
constexpr char kPublicKey4[] = "publickey4";
constexpr int64_t kStartSeconds4 = 0;
constexpr int32_t kStartNanos4 = 10;
constexpr int64_t kEndSeconds4 = 100;
constexpr int32_t kEndNanos4 = 30;
constexpr bool kForSelectedContacts4 = false;
constexpr char kMetadataEncryptionKey4[] = "metadataencryptionkey4";
constexpr char kEncryptedMetadataBytes4[] = "encryptedmetadatabytes4";
constexpr char kMetadataEncryptionKeyTag4[] = "metadataencryptionkeytag4";

std::string EncodeString(absl::string_view unencoded_string) {
  std::string result;
  absl::WebSafeBase64Escape(unencoded_string, &result);
  return result;
}

PublicCertificate CreatePublicCertificate(
    absl::string_view secret_id, absl::string_view secret_key,
    absl::string_view public_key, int64_t start_seconds, int32_t start_nanos,
    int64_t end_seconds, int32_t end_nanos, bool for_selected_contacts,
    absl::string_view metadata_encryption_key,
    absl::string_view encrypted_metadata_bytes,
    absl::string_view metadata_encryption_key_tag) {
  PublicCertificate cert;
  cert.set_secret_id(secret_id);
  cert.set_secret_key(secret_key);
  cert.set_public_key(public_key);
  cert.mutable_start_time()->set_seconds(start_seconds);
  cert.mutable_start_time()->set_nanos(start_nanos);
  cert.mutable_end_time()->set_seconds(end_seconds);
  cert.mutable_end_time()->set_nanos(end_nanos);
  cert.set_for_selected_contacts(for_selected_contacts);
  cert.set_metadata_encryption_key(metadata_encryption_key);
  cert.set_encrypted_metadata_bytes(encrypted_metadata_bytes);
  cert.set_metadata_encryption_key_tag(metadata_encryption_key_tag);
  return cert;
}

std::vector<NearbySharePrivateCertificate> CreatePrivateCertificates(
    size_t n, DeviceVisibility visibility) {
  std::vector<NearbySharePrivateCertificate> certs;
  certs.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    certs.emplace_back(visibility, absl::Now(), GetNearbyShareTestMetadata());
  }
  return certs;
}

absl::Time TimestampToTime(nearby::sharing::proto::Timestamp timestamp) {
  return absl::UnixEpoch() + absl::Seconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}

}  // namespace

class NearbyShareCertificateStorageImplTest : public ::testing::Test {
 public:
  NearbyShareCertificateStorageImplTest() = default;
  ~NearbyShareCertificateStorageImplTest() override = default;
  NearbyShareCertificateStorageImplTest(
      NearbyShareCertificateStorageImplTest&) = delete;
  NearbyShareCertificateStorageImplTest& operator=(
      NearbyShareCertificateStorageImplTest&) = delete;

  void SetUp() override {
    preference_manager_.Remove(
        prefs::kNearbySharingPublicCertificateExpirationDictName);
    preference_manager_.Remove(
        prefs::kNearbySharingPrivateCertificateListName);
  }

  std::map<std::string, PublicCertificate> PrepopulatePublicCertificates() {
    std::vector<PublicCertificate> pub_certs;
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId1, kSecretKey1, kPublicKey1, kStartSeconds1, kStartNanos1,
        kEndSeconds1, kEndNanos1, kForSelectedContacts1,
        kMetadataEncryptionKey1, kEncryptedMetadataBytes1,
        kMetadataEncryptionKeyTag1));
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId2, kSecretKey2, kPublicKey2, kStartSeconds2, kStartNanos2,
        kEndSeconds2, kEndNanos2, kForSelectedContacts2,
        kMetadataEncryptionKey2, kEncryptedMetadataBytes2,
        kMetadataEncryptionKeyTag2));
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId3, kSecretKey3, kPublicKey3, kStartSeconds3, kStartNanos3,
        kEndSeconds3, kEndNanos3, kForSelectedContacts3,
        kMetadataEncryptionKey3, kEncryptedMetadataBytes3,
        kMetadataEncryptionKeyTag3));
    std::map<std::string, PublicCertificate> entries;
    std::vector<std::pair<std::string, int64_t>> expirations;
    for (const auto& cert : pub_certs) {
      expirations.emplace_back(
          EncodeString(cert.secret_id()),
          absl::ToUnixNanos(TimestampToTime(cert.end_time())));
      entries.emplace(cert.secret_id(), std::move(cert));
    }
    preference_manager_.SetCertificateExpirationArray(
        prefs::kNearbySharingPublicCertificateExpirationDictName,
        expirations);
    return entries;
  }

  void CaptureBoolCallback(bool* dest, bool src) { *dest = src; }

  void PublicCertificateCallback(
      std::vector<PublicCertificate>* public_certificates,
      std::function<void()> complete, bool success,
      std::unique_ptr<std::vector<PublicCertificate>> result) {
    if (success && result) {
      public_certificates->swap(*result);
    }
    std::move(complete)();
  }

 protected:
  nearby::FakePreferenceManager preference_manager_;
};

TEST_F(NearbyShareCertificateStorageImplTest, InitializeRetrySucceed) {
  // This test only makes sense if initialization will be attempted at least
  // twice.
  if (kNearbyShareCertificateStorageMaxNumInitializeAttempts < 2) return;

  auto db = std::make_unique<StrictMock<MockPublicCertificateDb>>();
  MockPublicCertificateDb* mock_db = db.get();
  EXPECT_CALL(*mock_db, Initialize(_))
      .WillOnce(Invoke(
          [](absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus)&&>
                 callback) {
            std::move(callback)(MockPublicCertificateDb::InitStatus::kError);
          }))
      .WillRepeatedly(Invoke(
          [](absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus)&&>
                 callback) {
            std::move(callback)(MockPublicCertificateDb::InitStatus::kOk);
          }));
  EXPECT_CALL(*mock_db, Destroy(_))
      .WillOnce(Invoke([](absl::AnyInvocable<void(bool)&&> callback) {
        std::move(callback)(true);
      }));

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));

  bool clear_succeeded = false;
  // Use this to trigger call to Destroy.
  cert_store->ClearPublicCertificates([this, &clear_succeeded](bool success) {
    CaptureBoolCallback(&clear_succeeded, success);
  });

  EXPECT_TRUE(clear_succeeded);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, InitializeRetryFailed) {
  auto db = std::make_unique<StrictMock<MockPublicCertificateDb>>();
  MockPublicCertificateDb* mock_db = db.get();
  EXPECT_CALL(*mock_db, Initialize(_))
      .WillRepeatedly(Invoke(
          [](absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus)&&>
                 callback) {
            std::move(callback)(MockPublicCertificateDb::InitStatus::kError);
          }));

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));

  bool clear_succeeded = true;
  // Call to Destroy not made because of initialization failure.
  cert_store->ClearPublicCertificates([this, &clear_succeeded](bool success) {
    CaptureBoolCallback(&clear_succeeded, success);
  });

  EXPECT_FALSE(clear_succeeded);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest,
       InitializeCorruptDestroySucceeds) {
  auto db = std::make_unique<StrictMock<MockPublicCertificateDb>>();
  MockPublicCertificateDb* mock_db = db.get();
  EXPECT_CALL(*mock_db, Initialize(_))
      .WillOnce(Invoke(
          [](absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus)&&>
                 callback) {
            std::move(callback)(MockPublicCertificateDb::InitStatus::kCorrupt);
          }))
      .WillRepeatedly(Invoke(
          [](absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus)&&>
                 callback) {
            std::move(callback)(MockPublicCertificateDb::InitStatus::kOk);
          }));
  // Destroy called once from corrupted initialization and once from cal to
  // ClearPublicCertificates.
  EXPECT_CALL(*mock_db, Destroy(_))
      .Times(2)
      .WillRepeatedly(Invoke([](absl::AnyInvocable<void(bool)&&> callback) {
        std::move(callback)(true);
      }));

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));

  bool clear_succeeded = false;
  cert_store->ClearPublicCertificates([this, &clear_succeeded](bool success) {
    CaptureBoolCallback(&clear_succeeded, success);
  });

  EXPECT_TRUE(clear_succeeded);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, InitializeCorruptDestroyFails) {
  auto db = std::make_unique<StrictMock<MockPublicCertificateDb>>();
  MockPublicCertificateDb* mock_db = db.get();
  EXPECT_CALL(*mock_db, Initialize(_))
      .WillOnce(Invoke(
          [](absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus)&&>
                 callback) {
            std::move(callback)(MockPublicCertificateDb::InitStatus::kCorrupt);
          }));
  EXPECT_CALL(*mock_db, Destroy(_))
      .WillOnce(Invoke([](absl::AnyInvocable<void(bool)&&> callback) {
        std::move(callback)(false);
      }));

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));

  bool clear_succeeded = true;
  cert_store->ClearPublicCertificates([this, &clear_succeeded](bool success) {
    CaptureBoolCallback(&clear_succeeded, success);
  });

  EXPECT_FALSE(clear_succeeded);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, DeferredCallbackQueue) {
  absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus) &&>
      init_status_callback;
  absl::AnyInvocable<void(bool) &&> destroy_callback;
  absl::AnyInvocable<void(bool,
                          std::unique_ptr<std::vector<PublicCertificate>>) &&>
      load_callback;
  auto db = std::make_unique<StrictMock<MockPublicCertificateDb>>();
  MockPublicCertificateDb* mock_db = db.get();
  EXPECT_CALL(*mock_db, Initialize(_))
      .WillOnce(Invoke(
          [&](absl::AnyInvocable<void(MockPublicCertificateDb::InitStatus)&&>
                  callback) { init_status_callback = std::move(callback); }));
  EXPECT_CALL(*mock_db, Destroy(_))
      .WillOnce(Invoke([&](absl::AnyInvocable<void(bool)&&> callback) {
        destroy_callback = std::move(callback);
      }));
  EXPECT_CALL(*mock_db, LoadEntries(_))
      .WillOnce(Invoke(
          [&](absl::AnyInvocable<void(
                  bool, std::unique_ptr<std::vector<PublicCertificate>>)&&>
                  callback) { load_callback = std::move(callback); }));

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));

  bool clear_succeeded = false;
  std::vector<PublicCertificate> public_certificates;

  cert_store->ClearPublicCertificates([this, &clear_succeeded](bool success) {
    CaptureBoolCallback(&clear_succeeded, success);
  });
  cert_store->GetPublicCertificates(
      [this, &public_certificates, complete = []() {
      }](bool success, std::unique_ptr<std::vector<PublicCertificate>> result) {
        PublicCertificateCallback(&public_certificates, std::move(complete),
                                  success, std::move(result));
      });

  std::move(init_status_callback)(MockPublicCertificateDb::InitStatus::kOk);

  // These callbacks have to be posted to ensure that they run after the
  // deferred callbacks posted during initialization.
  std::move(destroy_callback)(true);
  std::move(load_callback)(true, nullptr);

  EXPECT_TRUE(clear_succeeded);
  EXPECT_TRUE(public_certificates.empty());
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, GetPublicCertificateIds) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();
  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  auto ids = cert_store->GetPublicCertificateIds();
  ASSERT_EQ(ids.size(), 3u);
  EXPECT_EQ(ids[0], kSecretId1);
  EXPECT_EQ(ids[1], kSecretId2);
  EXPECT_EQ(ids[2], kSecretId3);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, GetPublicCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  std::vector<PublicCertificate> public_certificates;
  cert_store->GetPublicCertificates([this, &public_certificates, complete = [] {
  }](bool success, std::unique_ptr<std::vector<PublicCertificate>> result) {
    PublicCertificateCallback(&public_certificates, std::move(complete),
                              success, std::move(result));
  });
  fake_db->InvokeLoadCallback(true);

  ASSERT_EQ(3u, public_certificates.size());
  for (const PublicCertificate& cert : public_certificates) {
    std::string expected_serialized, actual_serialized;
    ASSERT_TRUE(cert.SerializeToString(&actual_serialized));
    ASSERT_TRUE(fake_db->GetCertificatesMap()
                    .find(cert.secret_id())
                    ->second.SerializeToString(&expected_serialized));
    ASSERT_EQ(expected_serialized, actual_serialized);
  }
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, ReplacePublicCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();
  std::vector<PublicCertificate> new_certs = {
      CreatePublicCertificate(kSecretId4, kSecretKey4, kPublicKey4,
                              kStartSeconds4, kStartNanos4, kEndSeconds4,
                              kEndNanos4, kForSelectedContacts4,
                              kMetadataEncryptionKey4, kEncryptedMetadataBytes4,
                              kMetadataEncryptionKeyTag4),
  };
  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  bool succeeded = false;
  cert_store->ReplacePublicCertificates(
      new_certs, [this, &succeeded](bool success) {
        CaptureBoolCallback(&succeeded, success);
      });
  fake_db->InvokeDestroyCallback(true);
  fake_db->InvokeAddCallback(true);

  ASSERT_TRUE(succeeded);
  auto cert_map = fake_db->GetCertificatesMap();
  ASSERT_EQ(cert_map.size(), 1u);
  ASSERT_EQ(cert_map.count(kSecretId4), 1u);
  auto& cert = cert_map.find(kSecretId4)->second;
  EXPECT_EQ(cert.secret_key(), kSecretKey4);
  EXPECT_EQ(cert.public_key(), kPublicKey4);
  EXPECT_EQ(cert.start_time().seconds(), kStartSeconds4);
  EXPECT_EQ(cert.start_time().nanos(), kStartNanos4);
  EXPECT_EQ(cert.end_time().seconds(), kEndSeconds4);
  EXPECT_EQ(cert.end_time().nanos(), kEndNanos4);
  EXPECT_EQ(cert.for_selected_contacts(), kForSelectedContacts4);
  EXPECT_EQ(cert.metadata_encryption_key(), kMetadataEncryptionKey4);
  EXPECT_EQ(cert.encrypted_metadata_bytes(), kEncryptedMetadataBytes4);
  EXPECT_EQ(cert.metadata_encryption_key_tag(), kMetadataEncryptionKeyTag4);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, AddPublicCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();
  std::vector<PublicCertificate> new_certs = {
      CreatePublicCertificate(kSecretId3, kSecretKey2, kPublicKey2,
                              kStartSeconds2, kStartNanos2, kEndSeconds2,
                              kEndNanos2, kForSelectedContacts2,
                              kMetadataEncryptionKey2, kEncryptedMetadataBytes2,
                              kMetadataEncryptionKeyTag2),
      CreatePublicCertificate(kSecretId4, kSecretKey4, kPublicKey4,
                              kStartSeconds4, kStartNanos4, kEndSeconds4,
                              kEndNanos4, kForSelectedContacts4,
                              kMetadataEncryptionKey4, kEncryptedMetadataBytes4,
                              kMetadataEncryptionKeyTag4),
  };

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  bool succeeded = false;
  cert_store->AddPublicCertificates(new_certs,
                                    [this, &succeeded](bool success) {
                                      CaptureBoolCallback(&succeeded, success);
                                    });
  fake_db->InvokeAddCallback(true);

  ASSERT_TRUE(succeeded);
  auto cert_map = fake_db->GetCertificatesMap();
  ASSERT_EQ(cert_map.size(), 4u);
  ASSERT_EQ(cert_map.count(kSecretId3), 1u);
  ASSERT_EQ(cert_map.count(kSecretId4), 1u);
  auto& cert = cert_map.find(kSecretId3)->second;
  EXPECT_EQ(cert.secret_key(), kSecretKey2);
  EXPECT_EQ(cert.public_key(), kPublicKey2);
  EXPECT_EQ(cert.start_time().seconds(), kStartSeconds2);
  EXPECT_EQ(cert.start_time().nanos(), kStartNanos2);
  EXPECT_EQ(cert.end_time().seconds(), kEndSeconds2);
  EXPECT_EQ(cert.end_time().nanos(), kEndNanos2);
  EXPECT_EQ(cert.for_selected_contacts(), kForSelectedContacts2);
  EXPECT_EQ(cert.metadata_encryption_key(), kMetadataEncryptionKey2);
  EXPECT_EQ(cert.encrypted_metadata_bytes(), kEncryptedMetadataBytes2);
  EXPECT_EQ(cert.metadata_encryption_key_tag(), kMetadataEncryptionKeyTag2);
  cert = cert_map.find(kSecretId4)->second;
  EXPECT_EQ(cert.secret_key(), kSecretKey4);
  EXPECT_EQ(cert.public_key(), kPublicKey4);
  EXPECT_EQ(cert.start_time().seconds(), kStartSeconds4);
  EXPECT_EQ(cert.start_time().nanos(), kStartNanos4);
  EXPECT_EQ(cert.end_time().seconds(), kEndSeconds4);
  EXPECT_EQ(cert.end_time().nanos(), kEndNanos4);
  EXPECT_EQ(cert.for_selected_contacts(), kForSelectedContacts4);
  EXPECT_EQ(cert.metadata_encryption_key(), kMetadataEncryptionKey4);
  EXPECT_EQ(cert.encrypted_metadata_bytes(), kEncryptedMetadataBytes4);
  EXPECT_EQ(cert.metadata_encryption_key_tag(), kMetadataEncryptionKeyTag4);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, ClearPublicCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  bool succeeded = false;
  cert_store->ClearPublicCertificates([this, &succeeded](bool success) {
    CaptureBoolCallback(&succeeded, success);
  });
  fake_db->InvokeDestroyCallback(true);

  ASSERT_TRUE(succeeded);
  ASSERT_EQ(0u, fake_db->GetCertificatesMap().size());
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest,
       RemoveExpiredPrivateCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  std::vector<NearbySharePrivateCertificate> certs = CreatePrivateCertificates(
      3, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store->ReplacePrivateCertificates(certs);

  std::vector<absl::Time> expiration_times;
  for (const NearbySharePrivateCertificate& cert : certs) {
    expiration_times.push_back(cert.not_after());
  }
  std::sort(expiration_times.begin(), expiration_times.end());

  // Set the current time to exceed the expiration times of the first two
  // certificates.
  absl::Time now = expiration_times[1];

  cert_store->RemoveExpiredPrivateCertificates(now);

  certs = *cert_store->GetPrivateCertificates();
  ASSERT_EQ(1u, certs.size());
  for (const NearbySharePrivateCertificate& cert : certs) {
    EXPECT_LE(now, cert.not_after());
  }
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, RemoveExpiredPublicCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  std::vector<absl::Time> expiration_times;
  for (const auto& pair : fake_db->GetCertificatesMap()) {
    expiration_times.emplace_back(TimestampToTime(pair.second.end_time()));
  }
  std::sort(expiration_times.begin(), expiration_times.end());

  // The current time exceeds the expiration times of the first two
  // certificates even accounting for the expiration time tolerance
  // applied to public certificates to account for clock skew.
  absl::Time now = expiration_times[1] +
                   kNearbySharePublicCertificateValidityBoundOffsetTolerance;

  bool succeeded = false;
  cert_store->RemoveExpiredPublicCertificates(
      now, [this, &succeeded](bool success) {
        CaptureBoolCallback(&succeeded, success);
      });
  fake_db->InvokeRemoveCallback(true);

  ASSERT_TRUE(succeeded);
  auto cert_map = fake_db->GetCertificatesMap();
  ASSERT_EQ(cert_map.size(), 1u);
  for (const auto& pair : cert_map) {
    EXPECT_LE(now - kNearbySharePublicCertificateValidityBoundOffsetTolerance,
              TimestampToTime(pair.second.end_time()));
  }
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, ReplaceGetPrivateCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  auto certs_before = CreatePrivateCertificates(
      3, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store->ReplacePrivateCertificates(certs_before);
  auto certs_after = cert_store->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  ASSERT_EQ(certs_before.size(), certs_after->size());
  for (size_t i = 0; i < certs_before.size(); ++i) {
    EXPECT_EQ(certs_before[i].ToCertificateData(),
              (*certs_after)[i].ToCertificateData());
  }

  certs_before = CreatePrivateCertificates(
      1, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store->ReplacePrivateCertificates(certs_before);
  certs_after = cert_store->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  ASSERT_EQ(certs_before.size(), certs_after->size());
  for (size_t i = 0; i < certs_before.size(); ++i) {
    EXPECT_EQ(certs_before[i].ToCertificateData(),
              (*certs_after)[i].ToCertificateData());
  }
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, UpdatePrivateCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  std::vector<NearbySharePrivateCertificate> initial_certs =
      CreatePrivateCertificates(
          3, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store->ReplacePrivateCertificates(initial_certs);

  NearbySharePrivateCertificate cert_to_update = initial_certs[1];
  EXPECT_EQ(initial_certs[1].ToCertificateData(),
            cert_to_update.ToCertificateData());
  cert_to_update.EncryptMetadataKey();
  EXPECT_NE(initial_certs[1].ToCertificateData(),
            cert_to_update.ToCertificateData());

  cert_store->UpdatePrivateCertificate(cert_to_update);

  std::vector<NearbySharePrivateCertificate> new_certs =
      *cert_store->GetPrivateCertificates();
  EXPECT_EQ(initial_certs.size(), new_certs.size());
  for (size_t i = 0; i < new_certs.size(); ++i) {
    NearbySharePrivateCertificate expected_cert =
        i == 1 ? cert_to_update : initial_certs[i];
    EXPECT_EQ(expected_cert.ToCertificateData(),
              new_certs[i].ToCertificateData());
  }
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest,
       NextPrivateCertificateExpirationTime) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  auto certs = CreatePrivateCertificates(
      3, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store->ReplacePrivateCertificates(certs);
  std::optional<absl::Time> next_expiration =
      cert_store->NextPrivateCertificateExpirationTime();

  ASSERT_TRUE(next_expiration.has_value());
  bool found = false;
  for (auto& cert : certs) {
    EXPECT_GE(cert.not_after(), *next_expiration);
    if (cert.not_after() == *next_expiration) found = true;
  }
  EXPECT_TRUE(found);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest,
       NextPublicCertificateExpirationTime) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  std::optional<absl::Time> next_expiration =
      cert_store->NextPublicCertificateExpirationTime();

  ASSERT_TRUE(next_expiration.has_value());
  bool found = false;
  for (const auto& pair : fake_db->GetCertificatesMap()) {
    absl::Time curr_expiration = TimestampToTime(pair.second.end_time());
    EXPECT_GE(curr_expiration, *next_expiration);
    if (curr_expiration == *next_expiration) found = true;
  }
  EXPECT_TRUE(found);
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest, ClearPrivateCertificates) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  std::vector<NearbySharePrivateCertificate> certs_before =
      CreatePrivateCertificates(
          3, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  cert_store->ReplacePrivateCertificates(certs_before);
  cert_store->ClearPrivateCertificates();
  auto certs_after = cert_store->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  EXPECT_EQ(0u, certs_after->size());
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

TEST_F(NearbyShareCertificateStorageImplTest,
       ClearPrivateCertificatesOfVisibility) {
  auto db = std::make_unique<nearby::FakePublicCertificateDb>(
      PrepopulatePublicCertificates());
  nearby::FakePublicCertificateDb* fake_db = db.get();

  auto cert_store = NearbyShareCertificateStorageImpl::Factory::Create(
      preference_manager_, std::move(db));
  fake_db->InvokeInitStatusCallback(FakePublicCertificateDb::InitStatus::kOk);

  std::vector<NearbySharePrivateCertificate> certs_all_contacts =
      CreatePrivateCertificates(
          3, DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  std::vector<NearbySharePrivateCertificate> certs_selected_contacts =
      CreatePrivateCertificates(
          3, DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
  std::vector<NearbySharePrivateCertificate> all_certs;
  all_certs.reserve(certs_all_contacts.size() + certs_selected_contacts.size());
  all_certs.insert(all_certs.end(), certs_all_contacts.begin(),
                   certs_all_contacts.end());
  all_certs.insert(all_certs.end(), certs_selected_contacts.begin(),
                   certs_selected_contacts.end());

  // Remove all-contacts certs then selected-contacts certs.
  {
    cert_store->ReplacePrivateCertificates(all_certs);
    cert_store->ClearPrivateCertificatesOfVisibility(
        DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
    auto certs_after = cert_store->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    ASSERT_EQ(certs_selected_contacts.size(), certs_after->size());
    for (size_t i = 0; i < certs_selected_contacts.size(); ++i) {
      EXPECT_EQ(certs_selected_contacts[i].ToCertificateData(),
                (*certs_after)[i].ToCertificateData());
    }

    cert_store->ClearPrivateCertificatesOfVisibility(
        DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
    certs_after = cert_store->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    EXPECT_EQ(certs_after->size(), 0u);
  }

  // Remove selected-contacts certs then all-contacts certs.
  {
    cert_store->ReplacePrivateCertificates(all_certs);
    cert_store->ClearPrivateCertificatesOfVisibility(
        DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
    auto certs_after = cert_store->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    ASSERT_EQ(certs_all_contacts.size(), certs_after->size());
    for (size_t i = 0; i < certs_all_contacts.size(); ++i) {
      EXPECT_EQ(certs_all_contacts[i].ToCertificateData(),
                (*certs_after)[i].ToCertificateData());
    }

    cert_store->ClearPrivateCertificatesOfVisibility(
        DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
    certs_after = cert_store->GetPrivateCertificates();
    ASSERT_TRUE(certs_after.has_value());
    EXPECT_EQ(certs_after->size(), 0u);
  }
  EXPECT_THAT(cert_store.use_count(), Eq(1));
}

}  // namespace sharing
}  // namespace nearby
