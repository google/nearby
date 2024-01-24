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

#ifndef THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_H_

#include <stdint.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "internal/base/observer_list.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// The Nearby Share certificate manager maintains the local device's private
// certificates and contacts' public certificates. The manager communicates with
// the Nearby server to 1) download contacts' public certificates and 2) upload
// local device public certificates to be distributed to contacts.
//
// The class contains methods for performing crypto operations with the
// currently valid private certificate of a given visibility, such as signing a
// payload or generating an encrypted metadata key for an advertisement. For
// crypto operations related to public certificates, such as verifying a
// payload, find and decrypt the relevant certificate with
// DecryptPublicCertificate(), then use the
// NearbyShareDecryptedPublicCertificate class to perform the crypto operations.
// NOTE: The NearbySharePrivateCertificate class is not directly returned
// because storage needs to be updated whenever salts are consumed for metadata
// key encryption.
//
// Observers are notified of any changes to private/public certificates.
class NearbyShareCertificateManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnPublicCertificatesDownloaded() = 0;
    virtual void OnPrivateCertificatesChanged() = 0;
  };

  using CertDecryptedCallback =
      std::function<void(std::optional<NearbyShareDecryptedPublicCertificate>)>;

  NearbyShareCertificateManager();
  virtual ~NearbyShareCertificateManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts/Stops certificate task scheduling.
  void Start();
  void Stop();
  bool is_running() { return is_running_; }

  // Encrypts the metadata encryption key of the currently valid private
  // certificate with |visibility|. Returns absl::nullopt if there is no valid
  // private certificate with |visibility|, if the encryption fails, or if
  // there are no remaining salts.
  std::optional<NearbyShareEncryptedMetadataKey>
  EncryptPrivateCertificateMetadataKey(proto::DeviceVisibility visibility);

  // Signs the input |payload| using the currently valid private certificate
  // with |visibility|. Returns absl::nullopt if there is no valid private
  // certificate with |visibility| or if the signing was unsuccessful.
  std::optional<std::vector<uint8_t>> SignWithPrivateCertificate(
      proto::DeviceVisibility visibility,
      absl::Span<const uint8_t> payload) const;

  // Creates a hash of the |authentication_token| using the currently valid
  // private certificate. Returns absl::nullopt if there is no valid private
  // certificate with |visibility|.
  std::optional<std::vector<uint8_t>>
  HashAuthenticationTokenWithPrivateCertificate(
      proto::DeviceVisibility visibility,
      absl::Span<const uint8_t> authentication_token) const;

  // Returns all local device private certificates of |visibility| converted to
  // public certificates. The public certificates' for_selected_contacts fields
  // will be set to reflect the |visibility|. NOTE: Only certificates with the
  // requested visibility will be returned; if selected-contacts visibility is
  // passed in, the all-contacts visibility certificates will *not* be returned
  // as well.
  virtual std::vector<nearby::sharing::proto::PublicCertificate>
  GetPrivateCertificatesAsPublicCertificates(
      proto::DeviceVisibility visibility) = 0;

  // Returns in |callback| the public certificate that is able to be decrypted
  // using |encrypted_metadata_key|, and returns absl::nullopt if no such public
  // certificate exists.
  virtual void GetDecryptedPublicCertificate(
      NearbyShareEncryptedMetadataKey encrypted_metadata_key,
      CertDecryptedCallback callback) = 0;

  // Makes an RPC call to the Nearby server to retrieve all public certificates
  // available to the local device. These are also downloaded periodically.
  // Observers are notified when all public certificate downloads succeed via
  // OnPublicCertificatesDownloaded().
  virtual void DownloadPublicCertificates() = 0;

  // Clears all public certificates. when account logout,the public certificates
  // should be cleared.
  virtual void ClearPublicCertificates(std::function<void(bool)> callback) = 0;

  // Dump certificates ID information for troubleshooting.
  virtual std::string Dump() const = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  // Returns the currently valid private certificate with |visibility|, or
  // returns std::nullopt if one does not exist.
  virtual std::optional<NearbySharePrivateCertificate>
  GetValidPrivateCertificate(proto::DeviceVisibility visibility) const = 0;

  // Updates the existing record for |private_certificate|. If no such record
  // exists, this function does nothing.
  virtual void UpdatePrivateCertificateInStorage(
      const NearbySharePrivateCertificate& private_certificate) = 0;

  void NotifyPublicCertificatesDownloaded();
  void NotifyPrivateCertificatesChanged();

 private:
  bool is_running_ = false;
  nearby::ObserverList<Observer> observers_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_MANAGER_H_
