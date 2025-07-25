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

#include "sharing/contacts/fake_nearby_share_contact_manager.h"

#include <memory>
#include <vector>

#include "internal/platform/implementation/account_manager.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"

namespace nearby {
namespace sharing {

FakeNearbyShareContactManager::Factory::Factory() = default;

FakeNearbyShareContactManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareContactManager>
FakeNearbyShareContactManager::Factory::CreateInstance(
    Context* context, AccountManager& account_manager,
    nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory) {
  latest_nearby_client_factory_ = nearby_client_factory;
  latest_account_manager_ = &account_manager;

  auto instance = std::make_unique<FakeNearbyShareContactManager>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareContactManager::FakeNearbyShareContactManager() = default;

FakeNearbyShareContactManager::~FakeNearbyShareContactManager() = default;

void FakeNearbyShareContactManager::GetContacts(ContactsCallback callback) {}

}  // namespace sharing
}  // namespace nearby
