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

#include "sharing/contacts/nearby_share_contact_manager.h"

#include <stdint.h>

#include <vector>

#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

NearbyShareContactManager::NearbyShareContactManager() = default;

NearbyShareContactManager::~NearbyShareContactManager() = default;

void NearbyShareContactManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareContactManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareContactManager::Start() {
  if (is_running_) return;

  is_running_ = true;
  OnStart();
}

void NearbyShareContactManager::Stop() {
  if (!is_running_) return;

  is_running_ = false;
  OnStop();
}

void NearbyShareContactManager::NotifyContactsDownloaded(
    const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  for (Observer* observer : observers_.GetObservers()) {
    observer->OnContactsDownloaded(contacts,
                                   num_unreachable_contacts_filtered_out);
  }
}

void NearbyShareContactManager::NotifyContactsUploaded(
    bool did_contacts_change_since_last_upload) {
  for (Observer* observer : observers_.GetObservers()) {
    observer->OnContactsUploaded(did_contacts_change_since_last_upload);
  }
}

}  // namespace sharing
}  // namespace nearby
