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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// Stores local-device private certificates and remote-device public
// certificates. Provides methods to help manage certificate expiration. Due to
// the potentially large number of public certificates, some methods are
// asynchronous.
class NearbyShareCertificateStorage {
 public:
  using ResultCallback = std::function<void(bool)>;
  using PublicCertificateCallback = std::function<void(
      bool,
      std::unique_ptr<std::vector<nearby::sharing::proto::PublicCertificate>>)>;

  NearbyShareCertificateStorage() = default;
  virtual ~NearbyShareCertificateStorage() = default;

  // Returns the secret ids of all stored public certificates
  virtual std::vector<std::string> GetPublicCertificateIds() const = 0;

  // Returns all public certificates currently in storage. No RPC call is made.
  virtual void GetPublicCertificates(PublicCertificateCallback callback) = 0;

  // Returns all private certificates currently in storage. Will return
  // absl::nullopt if deserialization from prefs fails -- not expected to happen
  // under normal circumstances.
  virtual std::optional<std::vector<NearbySharePrivateCertificate>>
  GetPrivateCertificates() const = 0;

  // Returns the next time a certificate expires or absl::nullopt if no
  // certificates are present.
  std::optional<absl::Time> NextPrivateCertificateExpirationTime();
  virtual std::optional<absl::Time> NextPublicCertificateExpirationTime()
      const = 0;

  // Deletes existing private certificates and replaces them with
  // |private_certificates|.
  virtual void ReplacePrivateCertificates(
      absl::Span<const NearbySharePrivateCertificate> private_certificates) = 0;

  // Deletes existing public certificates and replaces them with
  // |public_certificates|.
  virtual void ReplacePublicCertificates(
      absl::Span<const nearby::sharing::proto::PublicCertificate>
          public_certificates,
      ResultCallback callback) = 0;

  // Overwrites an existing record with |private_certificate| if that record
  // has the same ID . If no such record exists in storage, no action is taken.
  // This method is necessary for updating the private certificate's list of
  // consumed salts.
  void UpdatePrivateCertificate(
      const NearbySharePrivateCertificate& private_certificate);

  // Adds public certificates, or replaces existing certificates
  // by secret_id
  virtual void AddPublicCertificates(
      absl::Span<const nearby::sharing::proto::PublicCertificate>
          public_certificates,
      ResultCallback callback) = 0;

  // Removes all private certificates from storage with expiration date after
  // |now|.
  void RemoveExpiredPrivateCertificates(absl::Time now);

  // Removes all public certificates from storage with expiration date after
  // |now|.
  virtual void RemoveExpiredPublicCertificates(absl::Time now,
                                               ResultCallback callback) = 0;

  // Delete all private certificates from memory and persistent storage.
  void ClearPrivateCertificates();

  // Delete private certificates with |visibility| from memory and persistent
  // storage.
  void ClearPrivateCertificatesOfVisibility(proto::DeviceVisibility visibility);

  // Delete all public certificates from memory and persistent storage.
  virtual void ClearPublicCertificates(ResultCallback callback) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
