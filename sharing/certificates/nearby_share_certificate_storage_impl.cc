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

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/private_certificate_data.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/internal/public/logging.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::api::PrivateCertificateData;
using ::nearby::sharing::api::PublicCertificateDatabase;

// Compare to leveldb_proto::Enums::InitStatus. Using a separate enum so that
// the values don't change.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum InitStatusMetric {
  kOK = 0,
  kNotInitialized = 1,
  kError = 2,
  kCorrupt = 3,
  kInvalidOperation = 4,
  kMaxValue = kInvalidOperation
};

std::string EncodeString(absl::string_view unencoded_string) {
  std::string result;
  absl::WebSafeBase64Escape(unencoded_string, &result);
  return result;
}

std::optional<std::string> DecodeString(const std::string* encoded_string) {
  std::string result;
  if (!encoded_string) return std::nullopt;

  if (absl::WebSafeBase64Unescape(*encoded_string, &result)) {
    return result;
  }
  return std::nullopt;
}

bool SortBySecond(const std::pair<std::string, absl::Time>& pair1,
                  const std::pair<std::string, absl::Time>& pair2) {
  return pair1.second < pair2.second;
}

NearbyShareCertificateStorageImpl::ExpirationList MergeExpirations(
    const NearbyShareCertificateStorageImpl::ExpirationList& old_exp,
    const NearbyShareCertificateStorageImpl::ExpirationList& new_exp) {
  // Remove duplicates with a preference for new entries.
  absl::btree_map<std::string, absl::Time> merged_map(new_exp.begin(),
                                                      new_exp.end());
  merged_map.insert(old_exp.begin(), old_exp.end());
  // Convert map to vector and sort by expiration time.
  NearbyShareCertificateStorageImpl::ExpirationList merged(merged_map.begin(),
                                                           merged_map.end());
  std::sort(merged.begin(), merged.end(), SortBySecond);
  return merged;
}

absl::Time TimestampToTime(nearby::sharing::proto::Timestamp timestamp) {
  return absl::UnixEpoch() + absl::Seconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}

}  // namespace

// static
NearbyShareCertificateStorageImpl::Factory*
    NearbyShareCertificateStorageImpl::Factory::test_factory_ = nullptr;

// static
std::shared_ptr<NearbyShareCertificateStorage>
NearbyShareCertificateStorageImpl::Factory::Create(
    PreferenceManager& preference_manager,
    std::unique_ptr<PublicCertificateDatabase> public_certificate_database) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        preference_manager, std::move(public_certificate_database));
  }

  auto storage = std::shared_ptr<NearbyShareCertificateStorageImpl>(
      new NearbyShareCertificateStorageImpl(
          preference_manager, std::move(public_certificate_database)));
  // Initialize cannot be called from c'tor since it tries to create a weak_ptr.
  storage->Initialize();
  return storage;
}

// static
void NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareCertificateStorageImpl::Factory::~Factory() = default;

NearbyShareCertificateStorageImpl::NearbyShareCertificateStorageImpl(
    PreferenceManager& preference_manager,
    std::unique_ptr<PublicCertificateDatabase> public_certificate_database)
    : preference_manager_(preference_manager),
      public_certificate_database_(std::move(public_certificate_database)) {
  FetchPublicCertificateExpirations();
}

NearbyShareCertificateStorageImpl::~NearbyShareCertificateStorageImpl() =
    default;

void NearbyShareCertificateStorageImpl::Initialize() {
  switch (init_status_) {
    case InitStatus::kUninitialized:
    case InitStatus::kFailed:
      num_initialize_attempts_++;
      if (num_initialize_attempts_ >
          kNearbyShareCertificateStorageMaxNumInitializeAttempts) {
        FinishInitialization(false);
        break;
      }

      NL_VLOG(1) << __func__
                 << ": Attempting to initialize public certificate "
                    "database. Number of attempts: "
                 << num_initialize_attempts_;
      public_certificate_database_->Initialize(
          [weak_this =
               weak_from_this()](PublicCertificateDatabase::InitStatus status) {
            if (auto storage = weak_this.lock()) {
              storage->OnDatabaseInitialized(absl::Now(), status);
            }
          });
      break;
    case InitStatus::kInitialized:
      NL_LOG(INFO) << __func__ << " already initialized.";
      break;
  }
}

void NearbyShareCertificateStorageImpl::DestroyAndReinitialize() {
  NL_LOG(ERROR) << __func__
                << ": Public certificate database corrupt. Erasing and "
                   "initializing new database.";
  init_status_ = InitStatus::kUninitialized;
  public_certificate_database_->Destroy(
      [weak_this = weak_from_this()](bool success) {
        if (auto storage = weak_this.lock()) {
          storage->OnDatabaseDestroyedReinitialize(
              [&](bool result) {
                NL_LOG(INFO)
                    << "Destroy and reinitialize database. result: " << result;
              },
              success);
        }
      });
}

void NearbyShareCertificateStorageImpl::OnDatabaseInitialized(
    absl::Time initialize_start_time,
    PublicCertificateDatabase::InitStatus status) {
  NL_LOG(INFO) << "Database is initialized for certificates. status="
               << static_cast<int>(status);
  switch (status) {
    case PublicCertificateDatabase::InitStatus::kOk:
      FinishInitialization(true);
      break;
    case PublicCertificateDatabase::InitStatus::kError:
      Initialize();
      break;
    case PublicCertificateDatabase::InitStatus::kCorrupt:
      DestroyAndReinitialize();
      break;
  }
}

void NearbyShareCertificateStorageImpl::FinishInitialization(bool success) {
  init_status_ = success ? InitStatus::kInitialized : InitStatus::kFailed;
  if (success) {
    // Need to reset the initialize attempts.
    num_initialize_attempts_ = 0;

    NL_VLOG(1) << __func__
               << "Public certificate database initialization succeeded.";
  } else {
    NL_LOG(ERROR) << __func__
                  << "Public certificate database initialization failed.";
  }

  // We run deferred callbacks even if initialization failed not to cause
  // possible client-side blocks of next calls to the database.
  while (!deferred_callbacks_.empty()) {
    auto deferred_task = std::move(deferred_callbacks_.front());
    deferred_callbacks_.pop();
    deferred_task();
  }
}

void NearbyShareCertificateStorageImpl::OnDatabaseDestroyedReinitialize(
    ResultCallback callback, bool success) {
  if (!success) {
    NL_LOG(ERROR) << __func__
                  << ": Failed to destroy public certificate database.";
    FinishInitialization(false);
    callback(false);
    return;
  }

  public_certificate_expirations_.clear();
  SavePublicCertificateExpirations();

  Initialize();
  callback(true);
}

void NearbyShareCertificateStorageImpl::OnDatabaseDestroyed(
    ResultCallback callback, bool success) {
  if (!success) {
    NL_LOG(ERROR) << __func__
                  << ": Failed to destroy public certificate database.";
    std::move(callback)(false);
    return;
  }

  public_certificate_expirations_.clear();
  SavePublicCertificateExpirations();

  std::move(callback)(true);
}

void NearbyShareCertificateStorageImpl::
    ReplacePublicCertificatesDestroyCallback(
        const std::vector<nearby::sharing::proto::PublicCertificate>&
            new_certificates,
        const ExpirationList& new_expirations, ResultCallback callback,
        bool proceed) {
  if (!proceed) {
    std::move(callback)(false);
    return;
  }

  NL_VLOG(1) << __func__ << ": Inserting " << new_certificates.size()
             << " public certificates.";

  public_certificate_database_->AddCertificates(
      absl::MakeConstSpan(new_certificates),
      [weak_this = weak_from_this(), new_expirations,
       callback = std::move(callback)](bool success) {
        if (auto storage = weak_this.lock()) {
          storage->ReplacePublicCertificatesUpdateEntriesCallback(
              std::make_unique<ExpirationList>(std::move(new_expirations)),
              std::move(callback), success);
        }
      });
}

void NearbyShareCertificateStorageImpl::
    ReplacePublicCertificatesUpdateEntriesCallback(
        std::unique_ptr<ExpirationList> expirations, ResultCallback callback,
        bool proceed) {
  if (!proceed) {
    NL_LOG(ERROR) << __func__ << ": Failed to replace public certificates.";
    std::move(callback)(false);
    return;
  }
  NL_VLOG(1) << __func__ << ": Successfully replaced public certificates.";

  NL_CHECK(expirations);
  public_certificate_expirations_.swap(*expirations);
  SavePublicCertificateExpirations();
  std::move(callback)(true);
}

void NearbyShareCertificateStorageImpl::AddPublicCertificatesCallback(
    std::unique_ptr<ExpirationList> new_expirations, ResultCallback callback,
    bool proceed) {
  if (!proceed) {
    NL_LOG(ERROR) << __func__ << ": Failed to add public certificates.";
    std::move(callback)(false);
    return;
  }
  NL_VLOG(1) << __func__ << ": Successfully added public certificates.";

  public_certificate_expirations_ =
      MergeExpirations(public_certificate_expirations_, *new_expirations);
  SavePublicCertificateExpirations();
  std::move(callback)(true);
}

void NearbyShareCertificateStorageImpl::RemoveExpiredPublicCertificatesCallback(
    const absl::flat_hash_set<std::string>& ids_to_remove,
    ResultCallback callback, bool proceed) {
  if (!proceed) {
    NL_LOG(ERROR) << __func__
                  << ": Failed to remove expired public certificates.";
    std::move(callback)(false);
    return;
  }
  NL_VLOG(1) << __func__
             << ": Expired public certificates successfully removed.";

  auto should_remove =
      [&](const std::pair<std::string, absl::Time>& pair) -> bool {
    return ids_to_remove.contains(pair.first);
  };

  public_certificate_expirations_.erase(
      std::remove_if(public_certificate_expirations_.begin(),
                     public_certificate_expirations_.end(), should_remove),
      public_certificate_expirations_.end());
  SavePublicCertificateExpirations();
  std::move(callback)(true);
}

std::vector<std::string>
NearbyShareCertificateStorageImpl::GetPublicCertificateIds() const {
  std::vector<std::string> ids;
  for (const auto& pair : public_certificate_expirations_) {
    ids.emplace_back(pair.first);
  }
  return ids;
}

void NearbyShareCertificateStorageImpl::GetPublicCertificates(
    PublicCertificateCallback callback) {
  if (init_status_ == InitStatus::kFailed) {
    std::move(callback)(false, nullptr);
    return;
  }

  if (init_status_ == InitStatus::kUninitialized) {
    deferred_callbacks_.push([this, callback = std::move(callback)] {
      GetPublicCertificates(std::move(callback));
    });
    return;
  }

  NL_VLOG(1) << __func__ << ": Calling LoadEntries on database.";
  public_certificate_database_->LoadEntries(std::move(callback));
}

std::optional<std::vector<NearbySharePrivateCertificate>>
NearbyShareCertificateStorageImpl::GetPrivateCertificates() const {
  std::vector<PrivateCertificateData> list =
      preference_manager_.GetPrivateCertificateArray(
          prefs::kNearbySharingPrivateCertificateListName);
  std::vector<NearbySharePrivateCertificate> certs;
  certs.reserve(list.size());
  for (const PrivateCertificateData& cert_data : list) {
    std::optional<NearbySharePrivateCertificate> cert(
        NearbySharePrivateCertificate::FromCertificateData(cert_data));
    if (!cert) return std::nullopt;

    certs.push_back(*std::move(cert));
  }
  return certs;
}

std::optional<absl::Time>
NearbyShareCertificateStorageImpl::NextPublicCertificateExpirationTime() const {
  if (public_certificate_expirations_.empty()) return std::nullopt;

  // |public_certificate_expirations_| is sorted by expiration date.
  return public_certificate_expirations_.front().second;
}

void NearbyShareCertificateStorageImpl::ReplacePrivateCertificates(
    absl::Span<const NearbySharePrivateCertificate> private_certificates) {
  std::vector<PrivateCertificateData> list;
  list.reserve(private_certificates.size());
  for (const NearbySharePrivateCertificate& cert : private_certificates) {
    list.push_back(cert.ToCertificateData());
  }
  preference_manager_.SetPrivateCertificateArray(
      prefs::kNearbySharingPrivateCertificateListName, list);
}

void NearbyShareCertificateStorageImpl::ReplacePublicCertificates(
    absl::Span<const nearby::sharing::proto::PublicCertificate>
        public_certificates,
    ResultCallback callback) {
  if (init_status_ == InitStatus::kFailed) {
    std::move(callback)(false);
    return;
  }

  if (init_status_ == InitStatus::kUninitialized) {
    deferred_callbacks_.push(
        [this, public_certificates, callback = std::move(callback)]() {
          ReplacePublicCertificates(public_certificates, std::move(callback));
        });

    return;
  }

  auto new_entries = std::vector<nearby::sharing::proto::PublicCertificate>();
  ExpirationList new_expirations;
  for (const nearby::sharing::proto::PublicCertificate& cert :
       public_certificates) {
    new_entries.emplace_back(cert);
    new_expirations.emplace_back(cert.secret_id(),
                                 TimestampToTime(cert.end_time()));
  }
  std::sort(new_expirations.begin(), new_expirations.end(), SortBySecond);

  NL_VLOG(1) << __func__ << ": Clearing public certificate database.";

  public_certificate_database_->Destroy(
      [weak_this = weak_from_this(), new_entries, new_expirations,
       callback = std::move(callback)](bool success) {
        if (auto storage = weak_this.lock()) {
          storage->ReplacePublicCertificatesDestroyCallback(
              new_entries, new_expirations, std::move(callback), success);
        }
      });
}

void NearbyShareCertificateStorageImpl::AddPublicCertificates(
    absl::Span<const nearby::sharing::proto::PublicCertificate>
        public_certificates,
    ResultCallback callback) {
  if (init_status_ == InitStatus::kFailed) {
    std::move(callback)(false);
    return;
  }

  if (init_status_ == InitStatus::kUninitialized) {
    deferred_callbacks_.push(
        [this, public_certificates, callback = std::move(callback)]() {
          AddPublicCertificates(public_certificates, std::move(callback));
        });
    return;
  }

  ExpirationList new_expirations;
  for (const nearby::sharing::proto::PublicCertificate& cert :
       public_certificates) {
    new_expirations.emplace_back(cert.secret_id(),
                                 TimestampToTime(cert.end_time()));
  }
  std::sort(new_expirations.begin(), new_expirations.end(), SortBySecond);

  NL_VLOG(1) << __func__
             << ": Calling UpdateEntries on public certificate database with "
             << public_certificates.size() << " certificates.";
  public_certificate_database_->AddCertificates(
      public_certificates, [weak_this = weak_from_this(), new_expirations,
                            callback = std::move(callback)](bool success) {
        if (auto storage = weak_this.lock()) {
          storage->AddPublicCertificatesCallback(
              std::make_unique<ExpirationList>(new_expirations),
              std::move(callback), success);
        }
      });
}

void NearbyShareCertificateStorageImpl::RemoveExpiredPublicCertificates(
    absl::Time now, ResultCallback callback) {
  if (init_status_ == InitStatus::kFailed) {
    std::move(callback)(false);
    return;
  }

  if (init_status_ == InitStatus::kUninitialized) {
    deferred_callbacks_.push([this, now, callback = std::move(callback)]() {
      RemoveExpiredPublicCertificates(now, std::move(callback));
    });
    return;
  }

  auto ids_to_remove = std::vector<std::string>();
  for (const auto& pair : public_certificate_expirations_) {
    // Because the list is sorted by expiration time, break as soon as we
    // encounter an unexpired certificate. Apply a tolerance when evaluating
    // whether the certificate is expired to account for clock skew between
    // devices. This conforms this the GmsCore implementation.
    if (!IsNearbyShareCertificateExpired(
            now,
            /*not_after=*/pair.second,
            /*use_public_certificate_tolerance=*/true)) {
      break;
    }

    ids_to_remove.emplace_back(pair.first);
  }
  if (ids_to_remove.empty()) {
    std::move(callback)(true);
    return;
  }

  NL_VLOG(1)
      << __func__
      << ": Calling UpdateEntries on public certificate database to remove "
      << ids_to_remove.size() << " expired certificates.";
  absl::flat_hash_set<std::string> remove_set(ids_to_remove.begin(),
                                              ids_to_remove.end());
  public_certificate_database_->RemoveCertificatesById(
      std::move(ids_to_remove),
      [weak_this = weak_from_this(), remove_set = std::move(remove_set),
       callback = std::move(callback)](bool success) {
        if (auto storage = weak_this.lock()) {
          storage->RemoveExpiredPublicCertificatesCallback(
              remove_set, std::move(callback), success);
        }
      });
}

void NearbyShareCertificateStorageImpl::ClearPublicCertificates(
    ResultCallback callback) {
  if (init_status_ == InitStatus::kFailed) {
    std::move(callback)(false);
    return;
  }

  NL_VLOG(1) << __func__ << ": Calling Destroy on public certificate database.";
  init_status_ = InitStatus::kUninitialized;
  public_certificate_database_->Destroy(
      [weak_this = weak_from_this(),
       callback = std::move(callback)](bool success) {
        if (auto storage = weak_this.lock()) {
          storage->OnDatabaseDestroyedReinitialize(callback, success);
        }
      });
}

bool NearbyShareCertificateStorageImpl::FetchPublicCertificateExpirations() {
  std::vector<std::pair<std::string, int64_t>> expirations =
      preference_manager_.GetCertificateExpirationArray(
          prefs::kNearbySharingPublicCertificateExpirationDictName);
  public_certificate_expirations_.clear();
  if (expirations.empty()) {
    return false;
  }

  public_certificate_expirations_.reserve(expirations.size());

  for (auto it = expirations.begin(); it != expirations.end(); ++it) {
    std::optional<std::string> id = DecodeString(&it->first);
    std::optional<absl::Time> expiration = absl::FromUnixNanos(it->second);
    if (!id || !expiration) return false;

    public_certificate_expirations_.emplace_back(*id, *expiration);
  }
  std::sort(public_certificate_expirations_.begin(),
            public_certificate_expirations_.end(), SortBySecond);

  return true;
}

void NearbyShareCertificateStorageImpl::SavePublicCertificateExpirations() {
  std::vector<std::pair<std::string, int64_t>> expirations;
  expirations.reserve(public_certificate_expirations_.size());
  for (const std::pair<std::string, absl::Time>& pair :
       public_certificate_expirations_) {
    expirations.emplace_back(EncodeString(pair.first),
                             absl::ToUnixNanos(pair.second));
  }

  preference_manager_.SetCertificateExpirationArray(
      prefs::kNearbySharingPublicCertificateExpirationDictName, expirations);
}

}  // namespace sharing
}  // namespace nearby
