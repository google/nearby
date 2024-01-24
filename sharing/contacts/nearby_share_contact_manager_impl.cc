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
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/crypto_cros/secure_hash.h"
#include "internal/platform/implementation/account_manager.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/contacts/nearby_share_contacts_sorter.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/base/encode.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::proto::Contact;
using ::nearby::sharing::proto::ContactRecord;
using ::nearby::sharing::proto::ListContactPeopleRequest;
using ::nearby::sharing::proto::ListContactPeopleResponse;

constexpr absl::Duration kContactUploadPeriod = absl::Hours(24);
constexpr absl::Duration kContactDownloadPeriod = absl::Hours(12);

// Removes contact IDs from the allowlist if they are not in |contacts|.
std::set<std::string> RemoveNonexistentContactsFromAllowlist(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<ContactRecord>& contacts) {
  std::set<std::string> new_allowed_contact_ids;
  for (const ContactRecord& contact : contacts) {
    if (allowed_contact_ids.find(contact.id()) != allowed_contact_ids.end())
      new_allowed_contact_ids.insert(contact.id());
  }
  return new_allowed_contact_ids;
}

// Converts a list of ContactRecord protos, along with the allowlist, into a
// list of Contact protos.
std::vector<Contact> ContactRecordsToContacts(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<ContactRecord>& contact_records) {
  std::vector<Contact> contacts;
  for (const ContactRecord& contact_record : contact_records) {
    bool is_selected = allowed_contact_ids.find(contact_record.id()) !=
                       allowed_contact_ids.end();
    for (const proto::Contact_Identifier& identifier :
         contact_record.identifiers()) {
      Contact contact;
      *contact.mutable_identifier() = identifier;
      contact.set_is_selected(is_selected);
      contacts.push_back(contact);
    }
  }
  return contacts;
}

Contact CreateLocalContact(absl::string_view profile_user_name) {
  Contact contact;
  contact.mutable_identifier()->set_account_name(
      std::string(profile_user_name));
  // Always consider your own account a selected contact.
  contact.set_is_selected(true);
  contact.set_is_self(true);
  return contact;
}

// Creates a hex-encoded hash of the contact data, implicitly including the
// allowlist, to be sent to the Nearby Share server. This hash is persisted and
// used to detect any changes to the user's contact list or allowlist since the
// last successful upload to the server. The hash is invariant under the
// ordering of |contacts|.
std::string ComputeHash(const std::vector<Contact>& contacts) {
  // To ensure that the hash is invariant under ordering of input |contacts|,
  // add all serialized protos to an ordered set. Then, incrementally calculate
  // the hash as we iterate through the set.
  std::set<std::string> serialized_contacts_set;
  for (const Contact& contact : contacts) {
    serialized_contacts_set.insert(contact.SerializeAsString());
  }

  std::unique_ptr<crypto::SecureHash> hasher =
      crypto::SecureHash::Create(crypto::SecureHash::Algorithm::SHA256);
  for (const std::string& serialized_contact : serialized_contacts_set) {
    hasher->Update(serialized_contact.data(), serialized_contact.size());
  }
  std::vector<uint8_t> hash(hasher->GetHashLength());
  hasher->Finish(hash.data(), hash.size());

  return nearby::utils::HexEncode(hash);
}

}  // namespace

// static
NearbyShareContactManagerImpl::Factory*
    NearbyShareContactManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareContactManager>
NearbyShareContactManagerImpl::Factory::Create(
    Context* context, PreferenceManager& preference_manager,
    AccountManager& account_manager,
    nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory,
    NearbyShareLocalDeviceDataManager* local_device_data_manager) {
  if (test_factory_) {
    return test_factory_->CreateInstance(context, account_manager,
                                         nearby_client_factory,
                                         local_device_data_manager);
  }

  return absl::WrapUnique(new NearbyShareContactManagerImpl(
      context, preference_manager, account_manager, nearby_client_factory,
      local_device_data_manager));
}

// static
void NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareContactManagerImpl::Factory::~Factory() = default;

NearbyShareContactManagerImpl::NearbyShareContactManagerImpl(
    Context* context, PreferenceManager& preference_manager,
    AccountManager& account_manager,
    nearby::sharing::api::SharingRpcClientFactory* nearby_client_factory,
    NearbyShareLocalDeviceDataManager* local_device_data_manager)
    : preference_manager_(preference_manager),
      account_manager_(account_manager),
      nearby_client_factory_(nearby_client_factory),
      nearby_share_client_(nearby_client_factory_->CreateInstance()),
      local_device_data_manager_(local_device_data_manager),
      periodic_contact_upload_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              context, preference_manager_, kContactUploadPeriod,
              /*retry_failures=*/false,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerPeriodicContactUploadName,
              [&] { OnPeriodicContactsUploadRequested(); })),
      contact_download_and_upload_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              context, preference_manager_, kContactDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerContactDownloadAndUploadName,
              [&] { DownloadContacts(); })),
      executor_(context->CreateSequencedTaskRunner()) {}

NearbyShareContactManagerImpl::~NearbyShareContactManagerImpl() = default;

void NearbyShareContactManagerImpl::OnContactsDownloadCompleted(
    std::vector<ContactRecord> contacts) {
  NL_LOG(INFO) << __func__ << ": Completed to download contacts from backend";
  size_t initial_num_contacts = contacts.size();
  contacts.erase(
      std::remove_if(contacts.begin(), contacts.end(),
                     [](const nearby::sharing::proto::ContactRecord& contact) {
                       return !contact.is_reachable();
                     }),
      contacts.end());
  uint32_t num_unreachable_contacts_filtered_out =
      initial_num_contacts - contacts.size();
  NL_VLOG(1) << __func__ << ": Removed "
             << num_unreachable_contacts_filtered_out
             << " unreachable contacts.";
  OnContactsDownloadSuccess(std::move(contacts),
                            num_unreachable_contacts_filtered_out);
}

void NearbyShareContactManagerImpl::ContactDownloadContext::FetchNextPage() {
  NL_LOG(INFO) << __func__ << ": Downloading page=" << page_number_++;
  ListContactPeopleRequest request;
  if (next_page_token_.has_value()) {
    request.set_page_token(*next_page_token_);
  }
  nearby_share_client_->ListContactPeople(
      request,
      [this](
          const absl::StatusOr<ListContactPeopleResponse>& response) mutable {
        if (!response.ok()) {
          NL_LOG(ERROR) << __func__ << ": Failed to download contacts.";
          std::move(download_failure_callback_)();
          return;
        }

        contacts_.insert(contacts_.end(), response->contact_records().begin(),
                        response->contact_records().end());

        if (response->next_page_token().empty()) {
          std::move(download_success_callback_)(std::move(contacts_));
          return;
        }
        // Continue with next page.
        next_page_token_ = response->next_page_token();
        FetchNextPage();
      });
}

void NearbyShareContactManagerImpl::DownloadContacts() {
  executor_->PostTask([this]() {
    NL_LOG(INFO) << __func__ << ": Start to download contacts";
    if (!is_running()) {
      NL_LOG(WARNING) << __func__
                      << ": Ignore to download contacts due to manager is not "
                         "running.";
      return;
    }

    std::vector<ContactRecord> contacts;
    if (!account_manager_.GetCurrentAccount().has_value()) {
      NL_LOG(WARNING)
          << __func__
          << ": Ignore to download certificates due to no login account.";
      OnContactsDownloadSuccess(contacts, 0);
      return;
    }

    // Currently Contacts download is synchronous.  It completes after
    // FetchNextPage() returns.
    auto context = std::make_unique<ContactDownloadContext>(
        nearby_share_client_.get(),
        absl::bind_front(
            &NearbyShareContactManagerImpl::OnContactsDownloadFailure, this),
        absl::bind_front(
            &NearbyShareContactManagerImpl::OnContactsDownloadCompleted, this));
    context->FetchNextPage();
  });
}

void NearbyShareContactManagerImpl::SetAllowedContacts(
    const std::set<std::string>& allowed_contact_ids) {
  // If the allowlist changed, re-upload contacts to Nearby server.
  if (SetAllowlist(allowed_contact_ids))
    contact_download_and_upload_scheduler_->MakeImmediateRequest();
}

void NearbyShareContactManagerImpl::OnStart() {
  periodic_contact_upload_scheduler_->Start();
  contact_download_and_upload_scheduler_->Start();
}

void NearbyShareContactManagerImpl::OnStop() {
  periodic_contact_upload_scheduler_->Stop();
  contact_download_and_upload_scheduler_->Stop();
}

std::set<std::string> NearbyShareContactManagerImpl::GetAllowedContacts()
    const {
  std::set<std::string> allowlist;
  for (const std::string& id : preference_manager_.GetStringArray(
           prefs::kNearbySharingAllowedContactsName, {})) {
    allowlist.insert(id);
  }
  return allowlist;
}

void NearbyShareContactManagerImpl::OnPeriodicContactsUploadRequested() {
  NL_VLOG(1) << __func__
             << ": Periodic Nearby Share contacts upload requested. "
             << "Upload will occur after next contacts download.";
}

void NearbyShareContactManagerImpl::OnContactsDownloadSuccess(
    std::vector<ContactRecord> contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  NL_LOG(INFO) << __func__ << ": Nearby Share download of " << contacts.size()
               << " contacts succeeded.";

  // Remove contacts from the allowlist that are not in the contact list.
  SetAllowlist(
      RemoveNonexistentContactsFromAllowlist(GetAllowedContacts(), contacts));

  // Notify observers that the contact list was downloaded.
  std::set<std::string> allowed_contact_ids = GetAllowedContacts();
  NotifyAllObserversContactsDownloaded(allowed_contact_ids, contacts,
                                       num_unreachable_contacts_filtered_out);

  std::vector<Contact> contacts_to_upload =
      ContactRecordsToContacts(GetAllowedContacts(), contacts);

  // Enable cross-device self-share by adding your account to the list of
  // contacts. It is also marked as a selected contact.
  std::optional<AccountManager::Account> account =
      account_manager_.GetCurrentAccount();

  if (!account.has_value()) {
    NL_LOG(WARNING) << __func__
                    << ": Profile user name is not valid; could not "
                    << "add self to list of contacts to upload.";
  } else {
    contacts_to_upload.push_back(CreateLocalContact(account->email));
  }

  std::string last_contact_upload_hash = preference_manager_.GetString(
      prefs::kNearbySharingContactUploadHashName, "");
  std::string contact_upload_hash = ComputeHash(contacts_to_upload);
  bool did_contacts_change_since_last_upload =
      contact_upload_hash != last_contact_upload_hash;

  if (did_contacts_change_since_last_upload) {
    NL_VLOG(1) << __func__ << ": Contact list or allowlist changed since last "
               << "successful upload to the Nearby Share server.";
  }

  // Request a contacts upload if the contact list or allowlist has changed
  // since the last successful upload. Also request an upload periodically.
  if (did_contacts_change_since_last_upload ||
      periodic_contact_upload_scheduler_->IsWaitingForResult()) {
    absl::Notification notification;
    bool upload_success = false;
    local_device_data_manager_->UploadContacts(std::move(contacts_to_upload),
                                               [&](bool success) {
                                                 upload_success = success;
                                                 notification.Notify();
                                               });

    notification.WaitForNotification();
    NL_LOG(INFO) << __func__ << ": Completed to upload contacts with result:"
                 << upload_success;

    OnContactsUploadFinished(did_contacts_change_since_last_upload,
                             contact_upload_hash, upload_success);
    return;
  }

  // No upload is needed.
  contact_download_and_upload_scheduler_->HandleResult(/*success=*/true);
}

void NearbyShareContactManagerImpl::OnContactsDownloadFailure() {
  NL_LOG(WARNING) << __func__ << ": Nearby Share contacts download failed.";

  contact_download_and_upload_scheduler_->HandleResult(/*success=*/false);
}

void NearbyShareContactManagerImpl::OnContactsUploadFinished(
    bool did_contacts_change_since_last_upload,
    absl::string_view contact_upload_hash, bool success) {
  NL_LOG(INFO) << __func__ << ": Upload of contacts to Nearby Share server "
               << (success ? "succeeded." : "failed.")
               << " Contact upload hash: " << contact_upload_hash;
  if (success) {
    // Only resolve the periodic upload request on success; let the
    // download-and-upload scheduler handle any failure retries. The periodic
    // upload scheduler will remember that it has an outstanding request even
    // after reboot.
    if (periodic_contact_upload_scheduler_->IsWaitingForResult()) {
      periodic_contact_upload_scheduler_->HandleResult(success);
    }

    std::string last_contact_upload_hash = preference_manager_.GetString(
        prefs::kNearbySharingContactUploadHashName, "");

    preference_manager_.SetString(prefs::kNearbySharingContactUploadHashName,
                                  contact_upload_hash);

    if (last_contact_upload_hash.empty()) {
      // If no contacts are uploaded before, set the flag to false in order to
      // prevent the certificate manager from regenerating certificates.
      NL_LOG(WARNING) << __func__
                      << ": Mark contacts change flag to false due to no "
                         "contacts upload before.";
      did_contacts_change_since_last_upload = false;
    }
    NotifyContactsUploaded(did_contacts_change_since_last_upload);
  }

  contact_download_and_upload_scheduler_->HandleResult(success);
}

bool NearbyShareContactManagerImpl::SetAllowlist(
    const std::set<std::string>& new_allowlist) {
  if (new_allowlist == GetAllowedContacts()) return false;

  std::vector<std::string> allowlist_value;
  allowlist_value.reserve(new_allowlist.size());
  for (const std::string& id : new_allowlist) {
    allowlist_value.push_back(id);
  }

  preference_manager_.SetStringArray(prefs::kNearbySharingAllowedContactsName,
                                     allowlist_value);
  return true;
}

void NearbyShareContactManagerImpl::NotifyAllObserversContactsDownloaded(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<ContactRecord>& contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  // Sort the contacts before sending the list to observers.
  std::vector<ContactRecord> sorted_contacts = contacts;
  SortNearbyShareContactRecords(&sorted_contacts);

  // First, notify NearbyShareContactManager::Observers.
  // Note: These are direct observers of the NearbyShareContactManager base
  // class, distinct from the mojo remote observers that we notify below.
  NotifyContactsDownloaded(allowed_contact_ids, sorted_contacts,
                           num_unreachable_contacts_filtered_out);
}

}  // namespace sharing
}  // namespace nearby
