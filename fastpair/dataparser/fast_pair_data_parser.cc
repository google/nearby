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
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "third_party/nearby//fastpair/crypto/fast_pair_decryption.h"
#include "fastpair/common/constant.h"
#include "fastpair/dataparser/fast_pair_decoder.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {
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

}  // namespace

void FastPairDataParser::GetHexModelIdFromServiceData(
    const std::vector<uint8_t>& service_data,
    GetHexModelIdFromServiceDataCallback callback) {
  callback.on_retrieved_cb(
      FastPairDecoder::HasModelId(&service_data)
          ? FastPairDecoder::GetHexModelIdFromServiceData(&service_data)
          : std::nullopt);
}

void FastPairDataParser::ParseDecryptedResponse(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_response_bytes,
    ParseDecryptResponseCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_response_bytes)) {
    callback.on_decrypted_cb(std::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_response_bytes, key, bytes);

  callback.on_decrypted_cb(
      FastPairDecryption::ParseDecryptResponse(key, bytes));
}

void FastPairDataParser::ParseDecryptedPasskey(
    const std::vector<uint8_t>& aes_key_bytes,
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    ParseDecryptPasskeyCallback callback) {
  if (!ValidateInputSizes(aes_key_bytes, encrypted_passkey_bytes)) {
    callback.on_decrypted_cb(std::nullopt);
    return;
  }

  std::array<uint8_t, kAesBlockByteSize> key;
  std::array<uint8_t, kEncryptedDataByteSize> bytes;
  ConvertVectorsToArrays(aes_key_bytes, encrypted_passkey_bytes, key, bytes);

  callback.on_decrypted_cb(FastPairDecryption::ParseDecryptPasskey(key, bytes));
}

}  // namespace fastpair
}  // namespace nearby
