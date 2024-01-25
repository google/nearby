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

#include "sharing/certificates/fake_nearby_share_certificate_storage.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/certificates/nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::proto::PublicCertificate;

FakeNearbyShareCertificateStorage::Factory::Factory() = default;

FakeNearbyShareCertificateStorage::Factory::~Factory() = default;

std::shared_ptr<NearbyShareCertificateStorage>
FakeNearbyShareCertificateStorage::Factory::CreateInstance(
    nearby::sharing::api::PreferenceManager& preference_manager,
    std::unique_ptr<nearby::sharing::api::PublicCertificateDatabase>
        public_certificate_database) {
  auto instance = std::make_shared<FakeNearbyShareCertificateStorage>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareCertificateStorage::ReplacePublicCertificatesCall::
    ReplacePublicCertificatesCall(
        const std::vector<PublicCertificate>& public_certificates,
        ResultCallback callback)
    : public_certificates(public_certificates), callback(std::move(callback)) {}

FakeNearbyShareCertificateStorage::ReplacePublicCertificatesCall::
    ReplacePublicCertificatesCall(ReplacePublicCertificatesCall&& other) =
        default;

FakeNearbyShareCertificateStorage::ReplacePublicCertificatesCall::
    ~ReplacePublicCertificatesCall() = default;

FakeNearbyShareCertificateStorage::AddPublicCertificatesCall::
    AddPublicCertificatesCall(
        const std::vector<PublicCertificate>& public_certificates,
        ResultCallback callback)
    : public_certificates(public_certificates), callback(std::move(callback)) {}

FakeNearbyShareCertificateStorage::AddPublicCertificatesCall::
    AddPublicCertificatesCall(AddPublicCertificatesCall&& other) = default;

FakeNearbyShareCertificateStorage::AddPublicCertificatesCall::
    ~AddPublicCertificatesCall() = default;

FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificatesCall::
    RemoveExpiredPublicCertificatesCall(absl::Time now, ResultCallback callback)
    : now(now), callback(std::move(callback)) {}

FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificatesCall::
    RemoveExpiredPublicCertificatesCall(
        RemoveExpiredPublicCertificatesCall&& other) = default;

FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificatesCall::
    ~RemoveExpiredPublicCertificatesCall() = default;

FakeNearbyShareCertificateStorage::FakeNearbyShareCertificateStorage() =
    default;

FakeNearbyShareCertificateStorage::~FakeNearbyShareCertificateStorage() =
    default;

std::vector<std::string>
FakeNearbyShareCertificateStorage::GetPublicCertificateIds() const {
  return public_certificate_ids_;
}

void FakeNearbyShareCertificateStorage::GetPublicCertificates(
    PublicCertificateCallback callback) {
  get_public_certificates_callbacks_.push_back(std::move(callback));
}

std::optional<std::vector<NearbySharePrivateCertificate>>
FakeNearbyShareCertificateStorage::GetPrivateCertificates() const {
  return private_certificates_;
}

std::optional<absl::Time>
FakeNearbyShareCertificateStorage::NextPublicCertificateExpirationTime() const {
  return next_public_certificate_expiration_time_;
}

void FakeNearbyShareCertificateStorage::ReplacePrivateCertificates(
    absl::Span<const NearbySharePrivateCertificate> private_certificates) {
  private_certificates_ = std::vector<NearbySharePrivateCertificate>(
      private_certificates.begin(), private_certificates.end());
}

void FakeNearbyShareCertificateStorage::ReplacePublicCertificates(
    absl::Span<const PublicCertificate> public_certificates,
    ResultCallback callback) {
  replace_public_certificates_calls_.emplace_back(
      std::vector<PublicCertificate>(public_certificates.begin(),
                                     public_certificates.end()),
      std::move(callback));
}

void FakeNearbyShareCertificateStorage::AddPublicCertificates(
    absl::Span<const PublicCertificate> public_certificates,
    ResultCallback callback) {
  add_public_certificates_calls_.emplace_back(
      std::vector<PublicCertificate>(public_certificates.begin(),
                                     public_certificates.end()),
      callback);
  if (is_sync_mode_) {
    callback(add_public_certificates_result_);
  }
}

void FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificates(
    absl::Time now, ResultCallback callback) {
  remove_expired_public_certificates_calls_.emplace_back(now, callback);
  if (is_sync_mode_) {
    callback(remove_expired_public_certificates_result_);
  }
}

void FakeNearbyShareCertificateStorage::ClearPublicCertificates(
    ResultCallback callback) {
  clear_public_certificates_callbacks_.push_back(std::move(callback));
}

void FakeNearbyShareCertificateStorage::SetPublicCertificateIds(
    absl::Span<const absl::string_view> ids) {
  public_certificate_ids_ = std::vector<std::string>(ids.begin(), ids.end());
}

void FakeNearbyShareCertificateStorage::SetNextPublicCertificateExpirationTime(
    std::optional<absl::Time> time) {
  next_public_certificate_expiration_time_ = time;
}

}  // namespace sharing
}  // namespace nearby
