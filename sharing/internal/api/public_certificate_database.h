// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PUBLIC_CERTIFICATE_DATABASE_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PUBLIC_CERTIFICATE_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/types/span.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby::sharing::api {

class PublicCertificateDatabase {
 public:
  enum class InitStatus {
    // Initialization successful.
    kOk = 0,
    // Failed to create database.
    kError = 1,
    // Existing database files are corrupt.  Caller should delete the database
    // and try to create it again.
    kCorrupt = 2,
  };

  PublicCertificateDatabase() = default;
  virtual ~PublicCertificateDatabase() = default;

  // Asynchronously initializes the object, which must have been created by the
  // DataManager::GetDataSet<T> function. |callback| can be invoked on an
  // executor thread when complete.
  virtual void Initialize(absl::AnyInvocable<void(InitStatus) &&> callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(
      absl::AnyInvocable<
          void(bool, std::unique_ptr<std::vector<
                         nearby::sharing::proto::PublicCertificate>>) &&>
          callback) = 0;

  // Asynchronously saves |certificates| to the database.
  // |callback| can be invoked on an executor thread when complete.
  virtual void AddCertificates(
      absl::Span<const nearby::sharing::proto::PublicCertificate> certificates,
      absl::AnyInvocable<void(bool) &&> callback) = 0;

  // Asynchronously deletes certificates with IDs in |ids_to_remove| from the
  // database.
  // |callback| can be invoked on an executor thread when complete.
  virtual void RemoveCertificatesById(
      std::vector<std::string> ids_to_remove,
      absl::AnyInvocable<void(bool) &&> callback) = 0;

  // Asynchronously destroys the database. Use this call only if the database
  // needs to be destroyed for this particular profile.
  virtual void Destroy(absl::AnyInvocable<void(bool) &&> callback) = 0;
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_PUBLIC_CERTIFICATE_DATABASE_H_
