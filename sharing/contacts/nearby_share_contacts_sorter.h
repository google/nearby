// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACTS_SORTER_H_
#define THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACTS_SORTER_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// Sort |contacts| by the following fields:
//  - person name or email address if name is empty (primary),
//  - email, even if this is also used as the primary (secondary),
//  - phone number (tertiary),
//  - contact record id (last resort; should always be unique).
//
// This sorted order is unique for a given |locale|, presuming every element of
// |contacts| has a unique ContactRecord::id(). The ordering between fields is
// locale-dependent. For example, 'Å' will be sorted with these 'A's for
// US-based sorting, whereas 'Å' will be sorted after 'Z' for Sweden-based
// sorting, because 'Å' comes after 'Z' in the Swedish alphabet. By default,
// |locale| is inferred from system settings.
void SortNearbyShareContactRecords(
    std::vector<nearby::sharing::proto::ContactRecord>* contacts,
    absl::string_view locale_string = "");

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACTS_SORTER_H_
