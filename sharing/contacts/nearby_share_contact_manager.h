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

#include <set>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "internal/base/observer_list.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// The Nearby Share contacts manager interfaces with the Nearby server in the
// following ways:
//   1) The user's contacts are downloaded from People API, using the Nearby
//   server as a proxy.
//   2) All the user's contacts are uploaded to Nearby server, along with an
//   indication of what contacts are allowed for selected-contacts visibility
//   mode. The Nearby server will distribute all-contacts visibility
//   certificates accordingly. For privacy reasons, the Nearby server needs to
//   explicitly receive the list of contacts from the device instead of pulling
//   them directly from People API.
//
// All contact data and update notifications are conveyed via observer methods;
// the manager does not return data directly from function calls.
class NearbyShareContactManager {
 public:
  using ContactsCallback = absl::AnyInvocable<
      void(absl::StatusOr<std::vector<nearby::sharing::proto::ContactRecord>>,
           uint32_t num_unreachable_contacts_filtered_out) &&>;

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnContactsDownloaded(
        const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
        uint32_t num_unreachable_contacts_filtered_out) = 0;
    virtual void OnContactsUploaded(
        bool did_contacts_change_since_last_upload) = 0;
  };

  NearbyShareContactManager();
  virtual ~NearbyShareContactManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts/Stops contact task scheduling.
  void Start();
  void Stop();
  bool is_running() { return is_running_; }

  // nearby_share::mojom::ContactManager:
  // Downloads the user's contact list from the server. The locally persisted
  // list of allowed contacts is reconciled with the newly downloaded contacts.
  // If the user's contact list or the allowlist has changed since the last
  // successful contacts upload to the Nearby Share server, via the UpdateDevice
  // RPC, an upload is requested. Contact downloads (and uploads if necessary)
  // are also scheduled periodically. The results are sent to observers via
  // OnContactsDownloaded(), and if an upload occurs, observers are notified via
  // OnContactsUploaded().
  virtual void DownloadContacts() = 0;

  // Retrieves the user's contact list from the server.
  virtual void GetContacts(ContactsCallback callback) = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  void NotifyContactsDownloaded(
      const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
      uint32_t num_unreachable_contacts_filtered_out);
  void NotifyContactsUploaded(bool did_contacts_change_since_last_upload);

 private:
  bool is_running_ = false;
  ObserverList<Observer> observers_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_
