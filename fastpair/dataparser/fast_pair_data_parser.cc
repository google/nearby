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

#include "fastpair/dataparser/fast_pair_data_parser.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "third_party/nearby//fastpair/crypto/fast_pair_decryption.h"
#include "fastpair/common/battery_notification.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/non_discoverable_advertisement.h"
#include "fastpair/dataparser/fast_pair_decoder.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr int kHeaderIndex = 0;
constexpr int kFieldTypeBitmask = 0b00001111;
constexpr int kFieldLengthBitmask = 0b11110000;
constexpr int kHeaderLength = 1;
constexpr int kFieldLengthOffset = 4;
constexpr int kFieldTypeAccountKeyFilter = 0;
constexpr int kFieldTypeAccountKeyFilterSalt = 1;
constexpr int kFieldTypeAccountKeyFilterNoNotification = 2;
constexpr int kFieldTypeBattery = 3;
constexpr int kFieldTypeBatteryNoNotification = 4;
constexpr int kAddressByteSize = 6;
constexpr int kMaxLengthOfSaltBytes = 2;

bool ValidateInputSizes(const std::vector<uint8_t>& aes_key_bytes,
                        const std::vector<uint8_t>& encrypted_bytes) {
  if (aes_key_bytes.size() != kAesBlockByteSize) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": AES key should have size = " << kAesBlockByteSize
                         << ", actual =  " << aes_key_bytes.size();
    return false;
  }

  if (encrypted_bytes.size() != kEncryptedDataByteSize) {
    NEARBY_LOGS(WARNING) << __func__ << ": Encrypted bytes should have size = "
                         << kEncryptedDataByteSize
                         << ", actual =  " << encrypted_bytes.size();
    return false;
  }

  return true;
}

void ConvertVectorsToArrays(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_bytes,
    std::array<uint8_t, kAesBlockByteSize>& out_aes_key_bytes,
    std::array<uint8_t, kEncryptedDataByteSize>& out_encrypted_bytes) {
  std::copy(aes_key_bytes.begin(), aes_key_bytes.end(),
            out_aes_key_bytes.begin());
  std::copy(encrypted_bytes.begin(), encrypted_bytes.end(),
            out_encrypted_bytes.begin());
}

std::vector<uint8_t> CopyFieldBytesToVector(
    const std::vector<uint8_t>& service_data,
    const absl::flat_hash_map<size_t, std::pair<size_t, size_t>>& field_indices,
    size_t key) {
  std::vector<uint8_t> out;
  DCHECK(field_indices.contains(key));

  auto indices = field_indices.find(key)->second;
  for (size_t i = indices.first; i < indices.second; i++) {
    out.push_back(service_data[i]);
  }
  return out;
}
}  // namespace

void FastPairDataParser::GetHexModelIdFromServiceData(
    const std::vector<uint8_t>& service_data,
    GetHexModelIdFromServiceDataCallback callback) {
  callback(FastPairDecoder::HasModelId(&service_data)
               ? FastPairDecoder::GetHexModelIdFromServiceData(&service_data)
               : std::nullopt);
}

void FastPairDataParser::ParseDecryptedResponse(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_response_bytes,
    ParseDecryptResponseCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_response_bytes)) {
    callback(std::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_response_bytes, key, bytes);
  callback(FastPairDecryption::ParseDecryptResponse(key, bytes));
}

void FastPairDataParser::ParseDecryptedPasskey(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    ParseDecryptPasskeyCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_passkey_bytes)) {
    callback(std::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_passkey_bytes, key, bytes);

  callback(FastPairDecryption::ParseDecryptPasskey(key, bytes));
}

void FastPairDataParser::ParseNotDiscoverableAdvertisement(
    absl::string_view fast_pair_service_data, absl::string_view address,
    ParseNotDiscoverableAdvertisementCallback callback) {
  std::vector<uint8_t> service_data;
  std::move(std::begin(fast_pair_service_data),
            std::end(fast_pair_service_data), std::back_inserter(service_data));
  if (service_data.empty() || FastPairDecoder::GetVersion(&service_data) != 0) {
    NEARBY_LOGS(WARNING) << " Invalid service data.";
    callback(std::nullopt);
    return;
  }

  absl::flat_hash_map<size_t, std::pair<size_t, size_t>> field_indices;
  size_t headerIndex = kHeaderIndex + kHeaderLength +
                       FastPairDecoder::GetIdLength(&service_data);

  while (headerIndex < service_data.size()) {
    size_t type = service_data[headerIndex] & kFieldTypeBitmask;
    size_t length =
        (service_data[headerIndex] & kFieldLengthBitmask) >> kFieldLengthOffset;
    size_t index = headerIndex + kHeaderLength;
    size_t end = index + length;

    if (end <= service_data.size()) {
      field_indices[type] = std::make_pair(index, end);
    }

    headerIndex = end;
  }

  // Account key filter bytes
  std::vector<uint8_t> account_key_filter_bytes;
  NonDiscoverableAdvertisement::Type show_ui =
      NonDiscoverableAdvertisement::Type::kNone;
  if (field_indices.contains(kFieldTypeAccountKeyFilter)) {
    account_key_filter_bytes = CopyFieldBytesToVector(
        service_data, field_indices, kFieldTypeAccountKeyFilter);
    show_ui = NonDiscoverableAdvertisement::Type::kShowUi;
  } else if (field_indices.contains(kFieldTypeAccountKeyFilterNoNotification)) {
    account_key_filter_bytes = CopyFieldBytesToVector(
        service_data, field_indices, kFieldTypeAccountKeyFilterNoNotification);
    show_ui = NonDiscoverableAdvertisement::Type::kHideUi;
  }

  if (account_key_filter_bytes.empty()) {
    NEARBY_LOGS(WARNING) << " Service data doesn't contain account key filter.";
    callback(std::nullopt);
    return;
  }

  // Salt bytes
  std::vector<uint8_t> salt_bytes;
  if (field_indices.contains(kFieldTypeAccountKeyFilterSalt)) {
    salt_bytes = CopyFieldBytesToVector(service_data, field_indices,
                                        kFieldTypeAccountKeyFilterSalt);
  }

  // https://developers.devsite.corp.google.com/nearby/fast-pair/specifications/service/provider#AccountKeyFilter
  if (salt_bytes.size() > kMaxLengthOfSaltBytes) {
    NEARBY_LOGS(WARNING) << " Parsed a salt field larger than two bytes: "
                         << salt_bytes.size();
    callback(std::nullopt);
    return;
  }

  if (salt_bytes.empty()) {
    NEARBY_LOGS(INFO)
        << __func__
        << ": missing salt field from device. Using device address instead.";
    std::array<uint8_t, kAddressByteSize> address_bytes;
    device::ParseBluetoothAddress(
        address, absl::MakeSpan(address_bytes.data(), kAddressByteSize));
    salt_bytes =
        std::vector<uint8_t>(address_bytes.begin(), address_bytes.end());
  }

  // Battery info bytes
  std::vector<uint8_t> battery_bytes;
  BatteryNotification::Type show_ui_for_battery =
      BatteryNotification::Type::kNone;
  if (field_indices.contains(kFieldTypeBattery)) {
    battery_bytes =
        CopyFieldBytesToVector(service_data, field_indices, kFieldTypeBattery);
    show_ui_for_battery = BatteryNotification::Type::kShowUi;
  } else if (field_indices.contains(kFieldTypeBatteryNoNotification)) {
    battery_bytes = CopyFieldBytesToVector(service_data, field_indices,
                                           kFieldTypeBatteryNoNotification);
    show_ui_for_battery = BatteryNotification::Type::kHideUi;
  }

  callback(NonDiscoverableAdvertisement(
      std::move(account_key_filter_bytes), show_ui, std::move(salt_bytes),
      BatteryNotification::FromBytes(battery_bytes, show_ui_for_battery)));
}

}  // namespace fastpair
}  // namespace nearby
