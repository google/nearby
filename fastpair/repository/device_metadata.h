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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_DEVICE_METADATA_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_DEVICE_METADATA_H_

#include <utility>

#include "fastpair/proto/fastpair_rpcs.proto.h"

namespace nearby {
namespace fastpair {
class DeviceMetadata {
 public:
  explicit DeviceMetadata(const proto::GetObservedDeviceResponse response)
      : response_(std::move(response)) {}
  DeviceMetadata(DeviceMetadata &&) = default;
  DeviceMetadata(const DeviceMetadata &) = delete;
  DeviceMetadata &operator=(const DeviceMetadata &) = delete;
  DeviceMetadata &operator=(DeviceMetadata &&) = delete;
  ~DeviceMetadata() = default;
  const proto::Device &GetDetails() { return response_.device(); }
  const proto::GetObservedDeviceResponse &GetResponse() { return response_; }

 private:
  const proto::GetObservedDeviceResponse response_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_DEVICE_METADATA_H_
