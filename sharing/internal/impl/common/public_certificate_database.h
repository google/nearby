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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_PUBLIC_CERTIFICATE_DATABASE_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_PUBLIC_CERTIFICATE_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby::sharing {

// ponytail: in-memory store. Public certificates are only downloaded from the
// Google backend (account visibility), which this build does not reach; add
// on-disk persistence when a certificate source exists.
class PublicCertificateDatabase : public api::PublicCertificateDatabase {
 public:
  void Initialize(absl::AnyInvocable<void(InitStatus) &&> callback) override;
  void LoadEntries(
      absl::AnyInvocable<
          void(bool, std::unique_ptr<std::vector<
                         nearby::sharing::proto::PublicCertificate>>) &&>
          callback) override;
  void LoadCertificate(
      absl::string_view id,
      absl::AnyInvocable<
          void(bool,
               std::unique_ptr<nearby::sharing::proto::PublicCertificate>) &&>
          callback) override;
  void AddCertificates(
      absl::Span<const nearby::sharing::proto::PublicCertificate> certificates,
      absl::AnyInvocable<void(bool) &&> callback) override;
  void RemoveCertificatesById(
      std::vector<std::string> ids_to_remove,
      absl::AnyInvocable<void(bool) &&> callback) override;
  void Destroy(absl::AnyInvocable<void(bool) &&> callback) override;

 private:
  absl::Mutex mutex_;
  absl::btree_map<std::string, nearby::sharing::proto::PublicCertificate>
      certificates_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_PUBLIC_CERTIFICATE_DATABASE_H_
