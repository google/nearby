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

#include "sharing/certificates/nearby_share_certificate_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <optional>
#include <ostream>
#include <vector>

#include "absl/time/time.h"
#include "sharing/certificates/common.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/base/encode.h"
#include "sharing/internal/public/logging.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::proto::DeviceVisibility;

std::optional<absl::Time>
NearbyShareCertificateStorage::NextPrivateCertificateExpirationTime() {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs || certs->empty()) return std::nullopt;

  absl::Time min_time = absl::InfiniteFuture();
  for (const NearbySharePrivateCertificate& cert : *certs)
    min_time = std::min(min_time, cert.not_after());

  return min_time;
}

void NearbyShareCertificateStorage::UpdatePrivateCertificate(
    const NearbySharePrivateCertificate& private_certificate) {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs) {
    NL_LOG(WARNING) << __func__ << ": No private certificates to update.";
    return;
  }

  auto it = std::find_if(
      certs->begin(), certs->end(),
      [&private_certificate](const NearbySharePrivateCertificate& cert) {
        return cert.id() == private_certificate.id();
      });
  if (it == certs->end()) {
    NL_VLOG(1) << __func__ << ": No private certificate with id="
               << nearby::utils::HexEncode(private_certificate.id());
    return;
  }

  NL_VLOG(1) << __func__ << ": Updating private certificate id="
             << nearby::utils::HexEncode(private_certificate.id());
  *it = private_certificate;
  ReplacePrivateCertificates(*certs);
}

void NearbyShareCertificateStorage::RemoveExpiredPrivateCertificates(
    absl::Time now) {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs) return;

  std::vector<NearbySharePrivateCertificate> unexpired_certs;
  for (const NearbySharePrivateCertificate& cert : *certs) {
    if (!IsNearbyShareCertificateExpired(
            now, cert.not_after(),
            /*use_public_certificate_tolerance=*/false)) {
      unexpired_certs.push_back(cert);
    }
  }

  size_t num_removed = certs->size() - unexpired_certs.size();
  if (num_removed == 0) return;

  NL_VLOG(1) << __func__ << ": Removing " << num_removed
             << " expired private certificates.";
  ReplacePrivateCertificates(unexpired_certs);
}

void NearbyShareCertificateStorage::ClearPrivateCertificates() {
  NL_VLOG(1) << __func__ << ": Removing all private certificates.";
  ReplacePrivateCertificates({});
}

void NearbyShareCertificateStorage::ClearPrivateCertificatesOfVisibility(
    DeviceVisibility visibility) {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs) return;

  bool were_certs_removed = false;
  std::vector<NearbySharePrivateCertificate> new_certs;
  for (const NearbySharePrivateCertificate& cert : *certs) {
    if (cert.visibility() == visibility) {
      were_certs_removed = true;
    } else {
      new_certs.push_back(cert);
    }
  }

  if (were_certs_removed) {
    NL_VLOG(1) << __func__
               << ": Removing all private certificates of visibility "
               << static_cast<int>(visibility);
    ReplacePrivateCertificates(new_certs);
  }
}

}  // namespace sharing
}  // namespace nearby
