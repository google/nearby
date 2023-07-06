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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_MOCK_FAST_PAIR_REPOSITORY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_MOCK_FAST_PAIR_REPOSITORY_H_

#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "fastpair/repository/fast_pair_repository.h"

namespace nearby {
namespace fastpair {

class MockFastPairRepository : public FastPairRepository {
 public:
  MOCK_METHOD(void, GetDeviceMetadata,
              (absl::string_view hex_model_id, DeviceMetadataCallback callback),
              (override));
};

}  // namespace fastpair
}  // namespace nearby


#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_MOCK_FAST_PAIR_REPOSITORY_H_
