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

#include "fastpair/common/account_key_filter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "absl/strings/string_view.h"
#include "fastpair/common/battery_notification.h"
#include "fastpair/common/non_discoverable_advertisement.h"
#include "internal/crypto_cros/sha2.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr int kBitsInByte = 8;

// SASS enabled peripherals create their Bloom filters with the first byte
// equals to either `the account key in use`
// or `the most recently used account key`
// see this spec:
// https://developers.google.com/nearby/fast-pair/early-access/specifications/extensions/sass#SassInUseAccountKey
constexpr uint8_t kRecentlyUsedByte = 0x05;
constexpr uint8_t kInUseByte = 0x06;
constexpr uint8_t kShowUi = 0b00110011;
constexpr uint8_t kHideUi = 0b00110100;

// Helper to AccountKeyFilter::IsAccountKeyInFilter().
// Performs the test to see if |data| is in |bit_sets|, a Bloom filter.
bool AccountKeyFilterChecker(const std::vector<uint8_t>& data,
                             const std::vector<uint8_t>& bit_sets) {
  std::array<uint8_t, 32> hashed = crypto::SHA256Hash(data);

  // Iterate over the hashed input in 4 byte increments, combine those 4
  // bytes into an unsigned int and use it as the index into our
  // |bit_sets|.
  for (size_t i = 0; i < hashed.size(); i += 4) {
    uint32_t hash = uint32_t{hashed[i]} << 24 | uint32_t{hashed[i + 1]} << 16 |
                    uint32_t{hashed[i + 2]} << 8 | hashed[i + 3];

    size_t num_bits = bit_sets.size() * kBitsInByte;
    size_t n = hash % num_bits;
    size_t byte_index = floor(n / kBitsInByte);
    size_t bit_index = n % kBitsInByte;
    bool is_set = (bit_sets[byte_index] >> bit_index) & 0x01;

    if (!is_set) return false;
  }
  NEARBY_LOGS(INFO) << __func__ << " The accountkey is possibly in set.";
  return true;
}

}  // namespace

AccountKeyFilter::AccountKeyFilter(
    const NonDiscoverableAdvertisement& advertisement)
    : bit_sets_(advertisement.account_key_filter) {
  salt_values_.resize(advertisement.salt.size());
  std::copy(advertisement.salt.begin(), advertisement.salt.end(),
            salt_values_.begin());

  // If the advertisement contains battery information, then that information
  // was also appended to the account keys to generate the filter. We need to
  // do the same when checking for matches, so save the values in salt_values_
  // for that purpose later.
  if (advertisement.battery_notification) {
    salt_values_.push_back(advertisement.battery_notification->type ==
                                   BatteryNotification::Type::kShowUi
                               ? kShowUi
                               : kHideUi);
    for (auto battery_info :
         advertisement.battery_notification->battery_infos) {
      salt_values_.push_back(battery_info.ToByte());
    }
  }
}

AccountKeyFilter::AccountKeyFilter(
    const std::vector<uint8_t>& account_key_filter_bytes,
    const std::vector<uint8_t>& salt_values)
    : bit_sets_(account_key_filter_bytes), salt_values_(salt_values) {}

bool AccountKeyFilter::IsPossiblyInSet(const AccountKey& account_key) {
  if (!account_key.Ok()) {
    NEARBY_LOGS(INFO) << __func__ << " Invalid account key.";
    return false;
  }
  if (bit_sets_.empty()) return false;
  // We first need to append the salt value to the input (see
  // https://developers.google.com/nearby/fast-pair/spec#AccountKeyFilter).
  std::vector<uint8_t> data(account_key.GetAsBytes().begin(),
                            account_key.GetAsBytes().end());
  for (auto& byte : salt_values_) data.push_back(byte);

  // We need to try account keys with different first bytes in case
  // the peripheral is SASS per
  // https://developers.google.com/nearby/fast-pair/early-access/specifications/extensions/sass#SassAdvertisingPayload
  if (AccountKeyFilterChecker(data, bit_sets_)) {
    return true;
  }
  data[0] = kRecentlyUsedByte;
  if (AccountKeyFilterChecker(data, bit_sets_)) {
    return true;
  }
  data[0] = kInUseByte;
  return AccountKeyFilterChecker(data, bit_sets_);
}

}  // namespace fastpair
}  // namespace nearby
