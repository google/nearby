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

#ifndef THIRD_PARTY_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_

#include "sharing/contacts/nearby_share_contact_manager.h"

namespace nearby {
namespace sharing {

// A fake implementation of NearbyShareContactManager.
class FakeNearbyShareContactManager : public NearbyShareContactManager {
 public:
  FakeNearbyShareContactManager() = default;
  ~FakeNearbyShareContactManager() override = default;

 private:
  void GetContacts(ContactsCallback callback) override {};
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_
