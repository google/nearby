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

#include "sharing/internal/impl/common/public_certificate_database.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby::sharing {

using ::nearby::sharing::proto::PublicCertificate;

void PublicCertificateDatabase::Initialize(
    absl::AnyInvocable<void(InitStatus) &&> callback) {
  std::move(callback)(InitStatus::kOk);
}

void PublicCertificateDatabase::LoadEntries(
    absl::AnyInvocable<void(bool,
                            std::unique_ptr<std::vector<PublicCertificate>>) &&>
        callback) {
  auto entries = std::make_unique<std::vector<PublicCertificate>>();
  {
    absl::MutexLock lock(mutex_);
    for (const auto& [id, certificate] : certificates_) {
      entries->push_back(certificate);
    }
  }
  std::move(callback)(true, std::move(entries));
}

void PublicCertificateDatabase::LoadCertificate(
    absl::string_view id,
    absl::AnyInvocable<void(bool, std::unique_ptr<PublicCertificate>) &&>
        callback) {
  std::unique_ptr<PublicCertificate> certificate;
  {
    absl::MutexLock lock(mutex_);
    auto it = certificates_.find(id);
    if (it != certificates_.end()) {
      certificate = std::make_unique<PublicCertificate>(it->second);
    }
  }
  bool found = certificate != nullptr;
  std::move(callback)(found, std::move(certificate));
}

void PublicCertificateDatabase::AddCertificates(
    absl::Span<const PublicCertificate> certificates,
    absl::AnyInvocable<void(bool) &&> callback) {
  {
    absl::MutexLock lock(mutex_);
    for (const PublicCertificate& certificate : certificates) {
      certificates_.insert_or_assign(certificate.secret_id(), certificate);
    }
  }
  std::move(callback)(true);
}

void PublicCertificateDatabase::RemoveCertificatesById(
    std::vector<std::string> ids_to_remove,
    absl::AnyInvocable<void(bool) &&> callback) {
  {
    absl::MutexLock lock(mutex_);
    for (const std::string& id : ids_to_remove) certificates_.erase(id);
  }
  std::move(callback)(true);
}

void PublicCertificateDatabase::Destroy(
    absl::AnyInvocable<void(bool) &&> callback) {
  {
    absl::MutexLock lock(mutex_);
    certificates_.clear();
  }
  std::move(callback)(true);
}

}  // namespace nearby::sharing
