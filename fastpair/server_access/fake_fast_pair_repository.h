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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAKE_FAST_PAIR_REPOSITORY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAKE_FAST_PAIR_REPOSITORY_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/server_access/fast_pair_repository.h"

namespace nearby {
namespace fastpair {
class FakeFastPairRepository : public FastPairRepository {
 public:
  FakeFastPairRepository() = default;
  FakeFastPairRepository(const FakeFastPairRepository&) = delete;
  FakeFastPairRepository& operator=(const FakeFastPairRepository&) = delete;
  ~FakeFastPairRepository() override = default;

  void SetFakeMetadata(absl::string_view hex_model_id, proto::Device metadata);
  void ClearFakeMetadata(absl::string_view hex_model_id);
  // FastPairRepository::
  void GetDeviceMetadata(absl::string_view hex_model_id,
                         DeviceMetadataCallback callback) override;

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<DeviceMetadata>> data_;
  DeviceMetadataCallback callback_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAKE_FAST_PAIR_REPOSITORY_H_
