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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_PUBLIC_CERTIFICATE_DB_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_PUBLIC_CERTIFICATE_DB_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/types/span.h"
#include "sharing/internal/api/public_certificate_database.h"

namespace nearby {

class FakePublicCertificateDb
    : public nearby::sharing::api::PublicCertificateDatabase {
 public:
  explicit FakePublicCertificateDb(
    std::map<std::string, nearby::sharing::proto::PublicCertificate> entries);
  ~FakePublicCertificateDb() override = default;

  void Initialize(
      absl::AnyInvocable<
          void(nearby::sharing::api::PublicCertificateDatabase::InitStatus) &&>
          callback) override;
  void LoadEntries(
      absl::AnyInvocable<
          void(bool, std::unique_ptr<std::vector<
                         nearby::sharing::proto::PublicCertificate>>) &&>
          callback) override;
  void AddCertificates(
      absl::Span<const nearby::sharing::proto::PublicCertificate> certificates,
      absl::AnyInvocable<void(bool) &&> callback) override;
  void RemoveCertificatesById(
      std::vector<std::string> ids_to_remove,
      absl::AnyInvocable<void(bool) &&> callback) override;
  void Destroy(absl::AnyInvocable<void(bool) &&> callback) override;

  std::map<std::string, nearby::sharing::proto::PublicCertificate>
  GetCertificatesMap() {
    return entries_;
  }

  // Invoke callbacks
  void InvokeInitStatusCallback(
      nearby::sharing::api::PublicCertificateDatabase::InitStatus init_status);
  void InvokeLoadCallback(bool success);
  void InvokeAddCallback(bool success);
  void InvokeRemoveCallback(bool success);
  void InvokeDestroyCallback(bool success);

 private:
  std::map<std::string, nearby::sharing::proto::PublicCertificate>
      entries_;
  absl::AnyInvocable<
          void(nearby::sharing::api::PublicCertificateDatabase::InitStatus) &&>
          init_status_callback_;
  absl::AnyInvocable<
          void(bool, std::unique_ptr<std::vector<
                         nearby::sharing::proto::PublicCertificate>>) &&>
          load_callback_;
  absl::AnyInvocable<void(bool) &&> add_callback_;
  absl::AnyInvocable<void(bool) &&> remove_callback_;
  absl::AnyInvocable<void(bool) &&> destroy_callback_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_PUBLIC_CERTIFICATE_DB_H_
