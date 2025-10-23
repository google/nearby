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

#ifndef THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_

#include <memory>

#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/task_runner.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"

namespace nearby {
namespace sharing {

class NearbyShareContactManagerImpl : public NearbyShareContactManager {
 public:
  NearbyShareContactManagerImpl(
      Context* context, AccountManager& account_manager,
      nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory);

  ~NearbyShareContactManagerImpl() override = default;

 private:
  // NearbyShareContactsManager:
  void GetContacts(ContactsCallback callback) override;

  AccountManager& account_manager_;
  std::unique_ptr<nearby::sharing::api::SharingRpcClient> nearby_share_client_;

  std::unique_ptr<TaskRunner> executor_ = nullptr;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
