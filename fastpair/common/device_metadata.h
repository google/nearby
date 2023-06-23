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

#include "fastpair/common/fast_pair_version.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"

namespace nearby {
namespace fastpair {
class DeviceMetadata {
 public:
  explicit DeviceMetadata(const proto::GetObservedDeviceResponse response)
      : response_(std::move(response)) {}
  DeviceMetadata(DeviceMetadata &&) = default;
  DeviceMetadata(const DeviceMetadata &) = default;
  DeviceMetadata &operator=(const DeviceMetadata &) = default;
  DeviceMetadata &operator=(DeviceMetadata &&) = default;
  ~DeviceMetadata() = default;
  const proto::Device &GetDetails() const { return response_.device(); }
  const proto::GetObservedDeviceResponse &GetResponse() const {
    return response_;
  }

  DeviceFastPairVersion GetFastPairVersion() const {
    // Anti-spoofing keys were introduced in Fast Pair v2, so if this isn't
    // available then the device is v1.
    if (GetDetails().anti_spoofing_key_pair().public_key().empty()) {
      return DeviceFastPairVersion::kV1;
    }
    return DeviceFastPairVersion::kHigherThanV1;
  }

 private:
  proto::GetObservedDeviceResponse response_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_DEVICE_METADATA_H_
