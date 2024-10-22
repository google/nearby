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

#include "sharing/internal/test/fake_public_certificate_db.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/types/span.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {

using ::nearby::sharing::proto::PublicCertificate;

FakePublicCertificateDb::FakePublicCertificateDb(
    std::map<std::string, PublicCertificate> entries)
    : entries_(std::move(entries)) {}

void FakePublicCertificateDb::Initialize(
    absl::AnyInvocable<void(PublicCertificateDatabase::InitStatus) &&>
        callback) {
  init_status_callback_ = std::move(callback);
}

void FakePublicCertificateDb::LoadEntries(
    absl::AnyInvocable<void(bool,
                            std::unique_ptr<std::vector<PublicCertificate>>) &&>
        callback) {
  load_callback_ = std::move(callback);
}

void FakePublicCertificateDb::AddCertificates(
    absl::Span<const PublicCertificate> certificates,
    absl::AnyInvocable<void(bool) &&> callback) {
  for (const auto& cert : certificates) {
    if (entries_.find(cert.secret_id()) != entries_.end()) {
      entries_.erase(cert.secret_id());
    }
    entries_.emplace(cert.secret_id(), cert);
  }
  add_callback_ = std::move(callback);
}

void FakePublicCertificateDb::RemoveCertificatesById(
    std::vector<std::string> ids_to_remove,
    absl::AnyInvocable<void(bool) &&> callback) {
  auto it = ids_to_remove.begin();
  while (it != ids_to_remove.end()) {
    entries_.erase(*it);
    ++it;
  }
  remove_callback_ = std::move(callback);
}

void FakePublicCertificateDb::Destroy(
    absl::AnyInvocable<void(bool) &&> callback) {
  entries_.clear();
  destroy_callback_ = std::move(callback);
}

void FakePublicCertificateDb::InvokeInitStatusCallback(
    PublicCertificateDatabase::InitStatus init_status) {
  std::move(init_status_callback_)(init_status);
}

void FakePublicCertificateDb::InvokeLoadCallback(bool success) {
  auto result = std::make_unique<std::vector<PublicCertificate>>();
  auto it = entries_.begin();
  while (it != entries_.end()) {
    result->push_back(it->second);
    ++it;
  }
  std::move(load_callback_)(success, std::move(result));
}

void FakePublicCertificateDb::InvokeAddCallback(bool success) {
  std::move(add_callback_)(success);
}

void FakePublicCertificateDb::InvokeRemoveCallback(bool success) {
  std::move(remove_callback_)(success);
}

void FakePublicCertificateDb::InvokeDestroyCallback(bool success) {
  std::move(destroy_callback_)(success);
}


}  // namespace nearby
