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

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/task_runner.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// Implementation of NearbyShareContactManager that persists the set of allowed
// contact IDs--for selected-contacts visibility mode--in prefs. Other
// contact data is downloaded from People API, via the NearbyShare server, as
// needed.
//
// The Nearby Share server must be explicitly informed of all contacts this
// device is aware of--needed for all-contacts visibility mode--as well as what
// contacts are allowed for selected-contacts visibility mode. These uploaded
// contact lists are used by the server to distribute the device's public
// certificates accordingly. This implementation persists a hash of the last
// uploaded contact data, and after every contacts download, a subsequent upload
// request is made if we detect that the contact list or allowlist has changed
// since the last successful upload. We also schedule periodic contact uploads
// just in case the server removed the record.
//
// In addition to supporting on-demand contact downloads, this implementation
// periodically checks in with the Nearby Share server to see if the user's
// contact list has changed since the last upload.
class NearbyShareContactManagerImpl : public NearbyShareContactManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareContactManager> Create(
        Context* context, AccountManager& account_manager,
        nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareContactManager> CreateInstance(
        Context* context, AccountManager& account_manager,
        nearby::sharing::api::SharingRpcClientFactory*
            nearby_client_factory) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareContactManagerImpl() override;

 private:
  // Class for maintaining a single instance of contacts download request.  It
  // is responsible for downloading all available pages and making the results
  // or error available.
  class ContactDownloadContext {
   public:
    ContactDownloadContext(
        nearby::sharing::api::SharingRpcClient* nearby_share_client,
        ContactsCallback download_callback)
        : nearby_share_client_(nearby_share_client),
          download_callback_(std::move(download_callback)) {}

    // Fetches the next page of contacts.
    // If |next_page_token_| is empty, it fetches the first page.
    // On successful download, if  page token in the response is empty, the
    // |download_callback_| is invoked with all downloaded contacts.
    void FetchNextPage();

   private:
    nearby::sharing::api::SharingRpcClient* const nearby_share_client_;
    std::optional<std::string> next_page_token_;
    int page_number_ = 1;
    std::vector<nearby::sharing::proto::ContactRecord> contacts_;
    ContactsCallback download_callback_;
  };

  NearbyShareContactManagerImpl(
      Context* context, AccountManager& account_manager,
      nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory);

  // NearbyShareContactsManager:
  void GetContacts(ContactsCallback callback) override;

  AccountManager& account_manager_;
  std::unique_ptr<nearby::sharing::api::SharingRpcClient> nearby_share_client_;

  std::unique_ptr<TaskRunner> executor_ = nullptr;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
