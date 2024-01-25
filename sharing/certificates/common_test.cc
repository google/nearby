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

#include "sharing/certificates/common.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "sharing/certificates/constants.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/certificates/nearby_share_private_certificate.h"
#include "sharing/certificates/test_util.h"
#include "sharing/common/nearby_share_enums.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::proto::DeviceVisibility;

TEST(NearbyShareCertificatesCommonTest, AuthenticationTokenHash) {
  EXPECT_EQ(
      GetNearbyShareTestPayloadHashUsingSecretKey(),
      ComputeAuthenticationTokenHash(
          GetNearbyShareTestPayloadToSign(),
          as_bytes(absl::MakeSpan(GetNearbyShareTestSecretKey()->key()))));
}

TEST(NearbyShareCertificatesCommonTest, ValidityPeriod_PrivateCertificate) {
  NearbySharePrivateCertificate cert = GetNearbyShareTestPrivateCertificate(
      DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
  const bool use_public_certificate_tolerance = false;

  // Set time before validity period.
  absl::Time now = cert.not_before() - absl::Milliseconds(1);
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at inclusive lower bound of validity period.
  now = cert.not_before();
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time in the middle of the validity period.
  now = cert.not_before() + (cert.not_after() - cert.not_before()) / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at non-inclusive upper bound of validity period.
  now = cert.not_after();
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period.
  now = cert.not_after() + absl::Milliseconds(1);
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));
}

TEST(NearbyShareCertificatesCommonTest, ValidityPeriod_PublicCertificate) {
  NearbyShareDecryptedPublicCertificate cert =
      *NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(
              DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS),
          GetNearbyShareTestEncryptedMetadataKey());
  const bool use_public_certificate_tolerance = true;

  // Set time before validity period, outside of tolerance.
  absl::Time now = cert.not_before() -
                   kNearbySharePublicCertificateValidityBoundOffsetTolerance -
                   absl::Milliseconds(1);
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time before validity period, at inclusive bound with tolerance.
  now = cert.not_before() -
        kNearbySharePublicCertificateValidityBoundOffsetTolerance;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time before validity period, inside of tolerance.
  now = cert.not_before() -
        kNearbySharePublicCertificateValidityBoundOffsetTolerance / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at inclusive lower bound of validity period.
  now = cert.not_before();
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time in the middle of the validity period.
  now = cert.not_before() + (cert.not_after() - cert.not_before()) / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at upper bound of validity period.
  now = cert.not_after();
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period, inside of tolerance.
  now = cert.not_after() +
        kNearbySharePublicCertificateValidityBoundOffsetTolerance / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period, at non-inclusive tolerance bound.
  now = cert.not_after() +
        kNearbySharePublicCertificateValidityBoundOffsetTolerance;
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period, outside of tolerance.
  now = cert.not_after() +
        kNearbySharePublicCertificateValidityBoundOffsetTolerance +
        absl::Milliseconds(1);
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
