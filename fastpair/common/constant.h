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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONSTANT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONSTANT_H_

namespace location {
namespace nearby {
namespace fastpair {

constexpr char kServiceId[] = "Fast Pair";
constexpr char kFastPairServiceUuid[] = "0000FE2C-0000-1000-8000-00805F9B34FB";
const char kKeyBasedPairingCharacteristicUuidV1[] = "1234";
const char kKeyBasedPairingCharacteristicUuidV2[] =
    "FE2C1234-8366-4814-8EB0-01DE32100BEA";
const char kPasskeyCharacteristicUuidV1[] = "1235";
const char kPasskeyCharacteristicUuidV2[] =
    "FE2C1235-8366-4814-8EB0-01DE32100BEA";
const char kAccountKeyCharacteristicUuidV1[] = "1236";
const char kAccountKeyCharacteristicUuidV2[] =
    "FE2C1236-8366-4814-8EB0-01DE32100BEA";

}  // namespace fastpair
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONSTANT_H_
