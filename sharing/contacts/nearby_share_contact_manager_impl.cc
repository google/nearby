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

#include "sharing/contacts/nearby_share_contact_manager_impl.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/account_manager.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::proto::ContactRecord;
using ::nearby::sharing::proto::ListContactPeopleRequest;
using ::nearby::sharing::proto::ListContactPeopleResponse;

void FilterOutUnreachableContacts(std::vector<ContactRecord>& contacts) {
  contacts.erase(
      std::remove_if(contacts.begin(), contacts.end(),
                     [](const nearby::sharing::proto::ContactRecord& contact) {
                       return !contact.is_reachable();
                     }),
      contacts.end());
}

}  // namespace

// static
NearbyShareContactManagerImpl::Factory*
    NearbyShareContactManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareContactManager>
NearbyShareContactManagerImpl::Factory::Create(
    Context* context, AccountManager& account_manager,
    nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory) {
  if (test_factory_) {
    return test_factory_->CreateInstance(context, account_manager,
                                         nearby_client_factory);
  }

  return absl::WrapUnique(new NearbyShareContactManagerImpl(
      context, account_manager, nearby_client_factory));
}

// static
void NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareContactManagerImpl::Factory::~Factory() = default;

NearbyShareContactManagerImpl::NearbyShareContactManagerImpl(
    Context* context, AccountManager& account_manager,
    nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory)
    : account_manager_(account_manager),
      nearby_share_client_(nearby_client_factory->CreateInstance()),
      executor_(context->CreateSequencedTaskRunner()) {}

NearbyShareContactManagerImpl::~NearbyShareContactManagerImpl() = default;

void NearbyShareContactManagerImpl::ContactDownloadContext::FetchNextPage() {
  LOG(INFO) << "Downloading contacts page=" << page_number_++;
  ListContactPeopleRequest request;
  if (next_page_token_.has_value()) {
    request.set_page_token(*next_page_token_);
  }
  nearby_share_client_->ListContactPeople(
      std::move(request),
      [this](
          const absl::StatusOr<ListContactPeopleResponse>& response) mutable {
        if (!response.ok()) {
          LOG(WARNING) << "Failed to download contacts: " << response.status();
          std::move(download_callback_)(
              response.status(), /*num_unreachable_contacts_filtered_out=*/0);
          return;
        }

        contacts_.insert(contacts_.end(), response->contact_records().begin(),
                         response->contact_records().end());

        if (response->next_page_token().empty()) {
          // We should filter here because we only care about contacts that we
          // can share with.
          uint32_t contacts_size = contacts_.size();
          FilterOutUnreachableContacts(contacts_);
          uint32_t num_unreachable_contacts_filtered_out =
              contacts_size - contacts_.size();
          std::move(download_callback_)(std::move(contacts_),
                                        num_unreachable_contacts_filtered_out);
          return;
        }
        // Continue with next page.
        next_page_token_ = response->next_page_token();
        FetchNextPage();
      });
}

void NearbyShareContactManagerImpl::GetContacts(ContactsCallback callback) {
  executor_->PostTask([this, callback = std::move(callback)]() mutable {
    LOG(INFO) << "Start downloading contacts";
    std::vector<ContactRecord> contacts;
    if (!account_manager_.GetCurrentAccount().has_value()) {
      LOG(WARNING) << "Ignore contacts download, no logged in account.";
      std::move(callback)(contacts,
                          /*num_unreachable_contacts_filtered_out=*/0);
      return;
    }

    absl::Notification notification;
    auto context = std::make_unique<ContactDownloadContext>(
        nearby_share_client_.get(),
        [&notification, callback = std::move(callback)](
            absl::StatusOr<std::vector<nearby::sharing::proto::ContactRecord>>
                contacts,
            uint32_t num_unreachable_contacts_filtered_out) mutable {
          std::move(callback)(std::move(contacts),
                              num_unreachable_contacts_filtered_out);
          notification.Notify();
        });
    context->FetchNextPage();
    // Wait for all pages of contacts to be downloaded.
    notification.WaitForNotification();
  });
}

}  // namespace sharing
}  // namespace nearby
