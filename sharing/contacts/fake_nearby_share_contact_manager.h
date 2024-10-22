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

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "internal/platform/implementation/account_manager.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/contacts/nearby_share_contact_manager_impl.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"

namespace nearby {
namespace sharing {

// A fake implementation of NearbyShareContactManager, along with a fake
// factory, to be used in tests. Stores parameters input into
// NearbyShareContactManager method calls. Use the notification methods from the
// base class--NotifyContactsDownloaded() and NotifyContactsUploaded()--to alert
// observers of changes; these methods are made public in this fake class.
class FakeNearbyShareContactManager : public NearbyShareContactManager {
 public:
  // Factory that creates FakeNearbyShareContactManager instances. Use in
  // NearbyShareContactManagerImpl::Factor::SetFactoryForTesting() in unit
  // tests.
  class Factory : public NearbyShareContactManagerImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareContactManager instances created by
    // CreateInstance().
    std::vector<FakeNearbyShareContactManager*>& instances() {
      return instances_;
    }

    nearby::sharing::api::SharingRpcClientFactory* latest_http_client_factory()
        const {
      return latest_nearby_client_factory_;
    }

    NearbyShareLocalDeviceDataManager* latest_local_device_data_manager()
        const {
      return latest_local_device_data_manager_;
    }

    AccountManager* latest_account_manager() const {
      return latest_account_manager_;
    }

   private:
    // NearbyShareContactManagerImpl::Factory:
    std::unique_ptr<NearbyShareContactManager> CreateInstance(
        Context* context, AccountManager& account_manager,
        nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory,
        NearbyShareLocalDeviceDataManager* local_device_data_manager) override;

    std::vector<FakeNearbyShareContactManager*> instances_;
    nearby::sharing::api::SharingRpcClientFactory*
        latest_nearby_client_factory_ = nullptr;
    NearbyShareLocalDeviceDataManager* latest_local_device_data_manager_ =
        nullptr;
    AccountManager* latest_account_manager_ = nullptr;
  };

  FakeNearbyShareContactManager();
  ~FakeNearbyShareContactManager() override;

  size_t num_download_contacts_calls() const {
    return num_download_contacts_calls_;
  }

  // Make protected methods from base class public in this fake class.
  using NearbyShareContactManager::NotifyContactsDownloaded;
  using NearbyShareContactManager::NotifyContactsUploaded;

 private:
  // NearbyShareContactsManager:
  void DownloadContacts() override;
  void OnStart() override;
  void OnStop() override;
  void GetContacts(ContactsCallback callback) override;

  size_t num_download_contacts_calls_ = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_
