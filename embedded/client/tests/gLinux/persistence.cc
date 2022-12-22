// Copyright 2022 Google LLC
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

#include <algorithm>
#include <map>
#include <vector>

#include "fakes.h"
#include "nearby_platform_persistence.h"

static std::map<nearby_fp_StoredKey, std::vector<uint8_t>> storage;
nearby_platform_status nearby_platform_LoadValue(nearby_fp_StoredKey key,
                                                 uint8_t* output,
                                                 size_t* length) {
  auto search = storage.find(key);
  if (search != storage.end()) {
    size_t value_length = search->second.size();
    if (value_length > *length) {
      *length = 0;
      return kNearbyStatusResourceExhausted;
    }
    *length = value_length;
    std::copy(search->second.begin(), search->second.end(), output);
  } else {
    // key not found.
    *length = 0;
  }
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_SaveValue(nearby_fp_StoredKey key,
                                                 const uint8_t* input,
                                                 size_t length) {
  storage[key].assign(input, input + length);
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_PersistenceInit() {
  storage.clear();
  return kNearbyStatusOK;
}

void nearby_test_fakes_SetAccountKeys(
    std::vector<AccountKeyPair>& account_key_pairs) {
  AccountKeyList list(account_key_pairs);
  nearby_test_fakes_SetAccountKeys(list);
}

void nearby_test_fakes_SetAccountKeys(const uint8_t* input, size_t length) {
  storage[kStoredKeyAccountKeyList].assign(input, input + length);
}

std::vector<uint8_t> nearby_test_fakes_GetRawAccountKeys() {
  return storage[kStoredKeyAccountKeyList];
}

void nearby_test_fakes_SetAccountKeys(AccountKeyList& keys) {
  auto v = keys.GetRawFormat();
  nearby_test_fakes_SetAccountKeys(v.data(), v.size());
}

AccountKeyList nearby_test_fakes_GetAccountKeys() {
  return AccountKeyList(nearby_test_fakes_GetRawAccountKeys());
}
