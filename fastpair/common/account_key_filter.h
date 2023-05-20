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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_ACCOUNT_KEY_FILTER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_ACCOUNT_KEY_FILTER_H_

#include <cstdint>
#include <vector>

#include "fastpair/common/account_key.h"
#include "fastpair/common/non_discoverable_advertisement.h"

namespace nearby {
namespace fastpair {

// Class which represents a Fast Pair Account Key account_key_filter
// https://developers.google.com/nearby/fast-pair/specifications/service/provider#AccountKeyFilter
class AccountKeyFilter {
 public:
  explicit AccountKeyFilter(const NonDiscoverableAdvertisement& advertisement);
  AccountKeyFilter(const std::vector<uint8_t>& account_key_filter_bytes,
                   const std::vector<uint8_t>& salt_values);
  ~AccountKeyFilter() = default;

  // Returns true if the `account_key` is possibly in the account key set
  // defined by the filter.
  // Return false if `account_key` is definitely not in set.
  bool IsPossiblyInSet(const AccountKey& account_key);

 private:
  std::vector<uint8_t> bit_sets_;
  std::vector<uint8_t> salt_values_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_ACCOUNT_KEY_FILTER_H_
