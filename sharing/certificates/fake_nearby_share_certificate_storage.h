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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_STORAGE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/certificates/nearby_share_certificate_storage.h"
#include "sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// A fake implementation of NearbyShareCertificateStorage, along with a fake
// factory, to be used in tests.
class FakeNearbyShareCertificateStorage : public NearbyShareCertificateStorage {
 public:
  // Factory that creates FakeNearbyShareCertificateStorage instances. Use
  // in NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting()
  // in unit tests.
  class Factory : public NearbyShareCertificateStorageImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareCertificateStorage instances created by
    // CreateInstance().
    std::vector<FakeNearbyShareCertificateStorage*>& instances() {
      return instances_;
    }

   private:
    // NearbyShareCertificateStorageImpl::Factory:
    std::shared_ptr<NearbyShareCertificateStorage> CreateInstance(
        nearby::sharing::api::PreferenceManager& preference_manager,
        std::unique_ptr<nearby::sharing::api::PublicCertificateDatabase>
            public_certificate_database) override;

    std::vector<FakeNearbyShareCertificateStorage*> instances_;
  };

  struct ReplacePublicCertificatesCall {
    ReplacePublicCertificatesCall(
        const std::vector<nearby::sharing::proto::PublicCertificate>&
            public_certificates,
        ResultCallback callback);
    ReplacePublicCertificatesCall(ReplacePublicCertificatesCall&& other);
    ~ReplacePublicCertificatesCall();

    std::vector<nearby::sharing::proto::PublicCertificate> public_certificates;
    ResultCallback callback;
  };

  struct AddPublicCertificatesCall {
    AddPublicCertificatesCall(
        const std::vector<nearby::sharing::proto::PublicCertificate>&
            public_certificates,
        ResultCallback callback);
    AddPublicCertificatesCall(AddPublicCertificatesCall&& other);
    ~AddPublicCertificatesCall();

    std::vector<nearby::sharing::proto::PublicCertificate> public_certificates;
    ResultCallback callback;
  };

  struct RemoveExpiredPublicCertificatesCall {
    RemoveExpiredPublicCertificatesCall(absl::Time now,
                                        ResultCallback callback);
    RemoveExpiredPublicCertificatesCall(
        RemoveExpiredPublicCertificatesCall&& other);
    ~RemoveExpiredPublicCertificatesCall();

    absl::Time now;
    ResultCallback callback;
  };

  FakeNearbyShareCertificateStorage();
  ~FakeNearbyShareCertificateStorage() override;

  // NearbyShareCertificateStorage:
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

  void SetPublicCertificateIds(absl::Span<const absl::string_view> ids);
  void SetNextPublicCertificateExpirationTime(std::optional<absl::Time> time);

  std::vector<PublicCertificateCallback>& get_public_certificates_callbacks() {
    return get_public_certificates_callbacks_;
  }

  std::vector<ReplacePublicCertificatesCall>&
  replace_public_certificates_calls() {
    return replace_public_certificates_calls_;
  }

  std::vector<AddPublicCertificatesCall>& add_public_certificates_calls() {
    return add_public_certificates_calls_;
  }

  std::vector<RemoveExpiredPublicCertificatesCall>&
  remove_expired_public_certificates_calls() {
    return remove_expired_public_certificates_calls_;
  }

  std::vector<ResultCallback>& clear_public_certificates_callbacks() {
    return clear_public_certificates_callbacks_;
  }

  void set_is_sync_mode(bool is_sync_mode) { is_sync_mode_ = is_sync_mode; }

  void SetAddPublicCertificatesResult(bool result) {
    add_public_certificates_result_ = result;
  }

  void SetRemoveExpiredPublicCertificatesResult(bool result) {
    remove_expired_public_certificates_result_ = result;
  }

 private:
  std::optional<absl::Time> next_public_certificate_expiration_time_;
  std::vector<std::string> public_certificate_ids_;
  std::optional<std::vector<NearbySharePrivateCertificate>>
      private_certificates_;
  std::vector<PublicCertificateCallback> get_public_certificates_callbacks_;
  std::vector<ReplacePublicCertificatesCall> replace_public_certificates_calls_;
  std::vector<AddPublicCertificatesCall> add_public_certificates_calls_;
  std::vector<RemoveExpiredPublicCertificatesCall>
      remove_expired_public_certificates_calls_;
  std::vector<ResultCallback> clear_public_certificates_callbacks_;
  bool is_sync_mode_ = false;
  bool add_public_certificates_result_ = false;
  bool remove_expired_public_certificates_result_ = false;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_FAKE_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
