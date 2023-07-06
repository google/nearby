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

#include "fastpair/repository/fast_pair_repository_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "fastpair/common/device_metadata.h"
#include "internal/platform/logging.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

FastPairRepositoryImpl::FastPairRepositoryImpl(FastPairClient* fast_pair_client)
    : fast_pair_client_(fast_pair_client) {}

void FastPairRepositoryImpl::GetDeviceMetadata(
    absl::string_view hex_model_id, DeviceMetadataCallback callback) {
  NEARBY_LOGS(INFO) << __func__ << " with model id= " << hex_model_id;
  executor_.Execute(
      "Get Device Metadata", [this, hex_model_id = std::string(hex_model_id),
                              callback = std::move(callback)]() mutable {
        NEARBY_LOGS(INFO) << __func__ << ": Start to get devic metadata.";
        proto::GetObservedDeviceRequest request;
        int64_t device_id;
        CHECK(absl::SimpleHexAtoi(hex_model_id, &device_id));
        request.set_device_id(device_id);
        request.set_mode(proto::GetObservedDeviceRequest::MODE_RELEASE);
        absl::StatusOr<proto::GetObservedDeviceResponse> response =
            fast_pair_client_->GetObservedDevice(request);
        if (response.ok()) {
          NEARBY_LOGS(WARNING) << "Got GetObservedDeviceResponse from backend.";
          metadata_cache_[hex_model_id] =
              std::make_unique<DeviceMetadata>(response.value());
          // TODO(b/289139378) : save device's metadata in local cache.
          callback(*metadata_cache_[hex_model_id]);
        } else {
          NEARBY_LOGS(WARNING)
              << "Failed to get GetObservedDeviceResponse from backend.";
          callback(std::nullopt);
        }
      });
}

}  // namespace fastpair
}  // namespace nearby
