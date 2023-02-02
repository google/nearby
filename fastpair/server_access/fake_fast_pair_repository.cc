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

#include "fastpair/server_access/fake_fast_pair_repository.h"

#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"

namespace nearby {
namespace fastpair {
void FakeFastPairRepository::SetFakeMetadata(absl::string_view hex_model_id,
                                             proto::Device metadata) {
  proto::GetObservedDeviceResponse response;
  *response.mutable_device() = metadata;
  data_[hex_model_id] = std::make_unique<DeviceMetadata>(response);
}

void FakeFastPairRepository::ClearFakeMetadata(absl::string_view hex_model_id) {
  data_.erase(hex_model_id);
}

void FakeFastPairRepository::GetDeviceMetadata(
    absl::string_view hex_model_id, DeviceMetadataCallback callback) {
  callback_ = std::move(callback);
  if (data_.contains(hex_model_id)) {
    std::move(callback_)(*data_[hex_model_id]);
    return;
  }
}

}  // namespace fastpair
}  // namespace nearby
