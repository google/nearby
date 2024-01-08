// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_PUBLIC_CERTIFICATE_DB_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_PUBLIC_CERTIFICATE_DB_H_

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "absl/functional/any_invocable.h"
#include "absl/types/span.h"
#include "sharing/internal/api/public_certificate_database.h"

namespace nearby::sharing::api {
class MockPublicCertificateDb : public PublicCertificateDatabase {
 public:
  MockPublicCertificateDb() = default;
  MockPublicCertificateDb(const PublicCertificateDatabase&) = delete;
  MockPublicCertificateDb& operator=(const PublicCertificateDatabase&) = delete;
  ~MockPublicCertificateDb() override = default;

  MOCK_METHOD(void, Initialize,
              (absl::AnyInvocable<void(InitStatus) &&> callback), (override));
  MOCK_METHOD(
      void, LoadEntries,
      (absl::AnyInvocable<
          void(bool, std::unique_ptr<std::vector<
                         nearby::sharing::proto::PublicCertificate>>) &&>
           callback),
      (override));
  MOCK_METHOD(
      void, AddCertificates,
      (absl::Span<const nearby::sharing::proto::PublicCertificate> certificates,
       absl::AnyInvocable<void(bool) &&> callback),
      (override));
  MOCK_METHOD(void, RemoveCertificatesById,
              (std::vector<std::string> ids_to_remove,
               absl::AnyInvocable<void(bool) &&> callback),
              (override));
  MOCK_METHOD(void, Destroy, (absl::AnyInvocable<void(bool) &&> callback),
              (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_PUBLIC_CERTIFICATE_DB_H_
