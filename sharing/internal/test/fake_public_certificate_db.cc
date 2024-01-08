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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/types/span.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {

using ::nearby::sharing::proto::PublicCertificate;

void FakePublicCertificateDb::Initialize(
    absl::AnyInvocable<void(PublicCertificateDatabase::InitStatus) &&>
        callback) {
  std::move(callback)(PublicCertificateDatabase::InitStatus::kOk);
}

void FakePublicCertificateDb::LoadEntries(
    absl::AnyInvocable<void(bool,
                            std::unique_ptr<std::vector<PublicCertificate>>) &&>
        callback) {
  auto result = std::make_unique<std::vector<PublicCertificate>>();
  auto it = entries_.begin();
  while (it != entries_.end()) {
    result->push_back(it->second);
    ++it;
  }

  std::move(callback)(true, std::move(result));
}

void FakePublicCertificateDb::AddCertificates(
    absl::Span<const PublicCertificate> certificates,
    absl::AnyInvocable<void(bool) &&> callback) {
  for (const auto& cert : certificates) {
    if (entries_.contains(cert.secret_id())) {
      entries_.erase(cert.secret_id());
    }
    entries_.emplace(cert.secret_id(), cert);
  }
  std::move(callback)(true);
}

void FakePublicCertificateDb::RemoveCertificatesById(
    std::vector<std::string> ids_to_remove,
    absl::AnyInvocable<void(bool) &&> callback) {
  auto it = ids_to_remove.begin();
  while (it != ids_to_remove.end()) {
    entries_.erase(*it);
    ++it;
  }
  std::move(callback)(true);
}

void FakePublicCertificateDb::Destroy(
    absl::AnyInvocable<void(bool) &&> callback) {
  entries_.clear();
  std::move(callback)(true);
}

}  // namespace nearby
