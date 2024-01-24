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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_

#include <stddef.h>

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/certificates/nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// Implements NearbyShareCertificateStorage using Prefs to store private
// certificates and LevelDB Proto to store public certificates. Must be
// initialized by calling Initialize before retrieving or storing certificates.
// Callbacks are guaranteed to not be invoked after
// NearbyShareCertificateStorageImpl is destroyed.
class NearbyShareCertificateStorageImpl : public NearbyShareCertificateStorage,
      public std::enable_shared_from_this<NearbyShareCertificateStorageImpl> {
 public:
  class Factory {
   public:
    static std::shared_ptr<NearbyShareCertificateStorage> Create(
        nearby::sharing::api::PreferenceManager& preference_manager,
        std::unique_ptr<nearby::sharing::api::PublicCertificateDatabase>
            public_certificate_database);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::shared_ptr<NearbyShareCertificateStorage> CreateInstance(
        nearby::sharing::api::PreferenceManager& preference_manager,
        std::unique_ptr<nearby::sharing::api::PublicCertificateDatabase>
            public_certificate_database) = 0;

   private:
    static Factory* test_factory_;
  };

  using ExpirationList = std::vector<std::pair<std::string, absl::Time>>;

  ~NearbyShareCertificateStorageImpl() override;
  NearbyShareCertificateStorageImpl(NearbyShareCertificateStorageImpl&) =
      delete;
  void operator=(NearbyShareCertificateStorageImpl&) = delete;

  // NearbyShareCertificateStorage
  std::vector<std::string> GetPublicCertificateIds() const override;
  void GetPublicCertificates(PublicCertificateCallback callback) override;
  std::optional<std::vector<NearbySharePrivateCertificate>>
  GetPrivateCertificates() const override;
  std::optional<absl::Time> NextPublicCertificateExpirationTime()
      const override;
  void ReplacePrivateCertificates(
      absl::Span<const NearbySharePrivateCertificate> private_certificates)
      override;
  void ReplacePublicCertificates(
      absl::Span<const nearby::sharing::proto::PublicCertificate>
          public_certificates,
      ResultCallback callback) override;
  void AddPublicCertificates(
      absl::Span<const nearby::sharing::proto::PublicCertificate>
          public_certificates,
      ResultCallback callback) override;
  void RemoveExpiredPublicCertificates(absl::Time now,
                                       ResultCallback callback) override;
  void ClearPublicCertificates(ResultCallback callback) override;

 private:
  enum class InitStatus { kUninitialized, kInitialized, kFailed };

  NearbyShareCertificateStorageImpl(
      nearby::sharing::api::PreferenceManager& preference_manager,
      std::unique_ptr<nearby::sharing::api::PublicCertificateDatabase>
          public_certificate_database);
  void Initialize();
  void OnDatabaseInitialized(
      absl::Time initialize_start_time,
      nearby::sharing::api::PublicCertificateDatabase::InitStatus
          init_dataset_status);
  void FinishInitialization(bool success);

  void OnDatabaseDestroyedReinitialize(ResultCallback callback, bool success);
  void OnDatabaseDestroyed(ResultCallback callback, bool success);

  void DestroyAndReinitialize();

  void ReplacePublicCertificatesDestroyCallback(
      const std::vector<nearby::sharing::proto::PublicCertificate>&
          new_certificates,
      const ExpirationList& new_expirations, ResultCallback callback,
      bool proceed);
  void ReplacePublicCertificatesUpdateEntriesCallback(
      std::unique_ptr<ExpirationList> expirations, ResultCallback callback,
      bool proceed);
  void AddPublicCertificatesCallback(
      std::unique_ptr<ExpirationList> new_expirations, ResultCallback callback,
      bool proceed);
  void RemoveExpiredPublicCertificatesCallback(
      const absl::flat_hash_set<std::string>& ids_to_remove,
      ResultCallback callback, bool proceed);

  bool FetchPublicCertificateExpirations();
  void SavePublicCertificateExpirations();

  nearby::sharing::api::PreferenceManager& preference_manager_;
  InitStatus init_status_ = InitStatus::kUninitialized;
  size_t num_initialize_attempts_ = 0;
  std::unique_ptr<nearby::sharing::api::PublicCertificateDatabase>
      public_certificate_database_;
  ExpirationList public_certificate_expirations_;

  std::queue<std::function<void()>> deferred_callbacks_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_IMPL_H_
