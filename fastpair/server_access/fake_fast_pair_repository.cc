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
#include <string>
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
  executor_.Execute([callback = std::move(callback), this,
                     hex_model_id = std::string(hex_model_id)]() mutable {
    if (data_.contains(hex_model_id)) {
      callback(*data_[hex_model_id]);
    }
  });
}

std::unique_ptr<FakeFastPairRepository> FakeFastPairRepository::Create(
    absl::string_view model_id, absl::string_view public_anti_spoof_key) {
  std::string decoded_key;
  absl::Base64Unescape(public_anti_spoof_key, &decoded_key);
  proto::Device metadata;
  auto repository = std::make_unique<FakeFastPairRepository>();
  metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
  repository->SetFakeMetadata(model_id, metadata);
  return repository;
}

}  // namespace fastpair
}  // namespace nearby
