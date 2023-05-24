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

#include "fastpair/dataparser/fast_pair_decoder.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/escaping.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr int kHeaderIndex = 0;
constexpr int kHeaderLength = 1;
constexpr int kHeaderLengthBitmask = 0b00011110;
constexpr int kHeaderLengthOffset = 1;
constexpr int kHeaderVersionBitmask = 0b11100000;
constexpr int kHeaderVersionOffset = 5;
constexpr int kMinModelIdLength = 3;
constexpr int kMaxModelIdLength = 14;
}  // namespace

int FastPairDecoder::GetIdLength(const std::vector<uint8_t>* service_data) {
  return service_data->size() == kMinModelIdLength
             ? kMinModelIdLength
             : ((*service_data)[kHeaderIndex] & kHeaderLengthBitmask) >>
                   kHeaderLengthOffset;
}

int FastPairDecoder::GetVersion(const std::vector<uint8_t>* service_data) {
  return service_data->size() == kMinModelIdLength
             ? 0
             : ((*service_data)[kHeaderIndex] & kHeaderVersionBitmask) >>
                   kHeaderVersionOffset;
}

bool IsIdLengthValid(const std::vector<uint8_t>* service_data) {
  int id_length = FastPairDecoder::GetIdLength(service_data);
  return kMinModelIdLength <= id_length && id_length <= kMaxModelIdLength &&
         id_length + kHeaderLength <= static_cast<int>(service_data->size());
}

bool FastPairDecoder::HasModelId(const std::vector<uint8_t>* service_data) {
  return service_data != nullptr &&
         (service_data->size() == kMinModelIdLength ||
          // Header byte exists. We support only format version 0. (A different
          // version indicates a breaking change in the format.)
          (service_data->size() > kMinModelIdLength &&
           GetVersion(service_data) == 0 && IsIdLengthValid(service_data)));
}

std::optional<std::string> FastPairDecoder::GetHexModelIdFromServiceData(
    const std::vector<uint8_t>* service_data) {
  if (service_data == nullptr || service_data->size() < kMinModelIdLength) {
    return std::nullopt;
  }

  if (service_data->size() == kMinModelIdLength) {
    // If the size is 3, all the bytes are the ID,
    std::vector<uint8_t> bytes = *service_data;
    std::string model_id(bytes.begin(), bytes.end());
    return absl::BytesToHexString(model_id);
  }
  // Otherwise, the first byte is a header which contains the length of the
  // big-endian model ID that follows. The model ID will be trimmed if it
  // contains leading zeros.
  int id_index = 1;
  int end = id_index + GetIdLength(service_data);

  // Ignore leading zeros.
  while ((*service_data)[id_index] == 0 && end - id_index > kMinModelIdLength) {
    id_index++;
  }

  // Copy appropriate bytes to new array.
  int bytes_size = end - id_index;
  std::vector<uint8_t> bytes;
  bytes.reserve(bytes_size);
  for (int i = 0; i < bytes_size; i++) {
    bytes.push_back((*service_data)[i + id_index]);
  }
  std::string model_id(bytes.begin(), bytes.end());
  return absl::BytesToHexString(model_id);
}

}  // namespace fastpair
}  // namespace nearby
