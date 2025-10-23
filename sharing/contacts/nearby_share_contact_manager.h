// Copyright 2021-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_

#include <stdint.h>

#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// The Nearby Share contacts manager retrieves the user's contact list from the
// server.
class NearbyShareContactManager {
 public:
  using ContactsCallback = absl::AnyInvocable<
      void(absl::StatusOr<std::vector<nearby::sharing::proto::ContactRecord>>,
           uint32_t num_unreachable_contacts_filtered_out) &&>;

  virtual ~NearbyShareContactManager() = default;

  // Retrieves the user's contact list from the server.
  virtual void GetContacts(ContactsCallback callback) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_
