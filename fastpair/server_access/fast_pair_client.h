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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_CLIENT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_CLIENT_H_

#include "absl/status/statusor.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"

namespace nearby {
namespace fastpair {

// FastPairClient is used to access Fast Pair backend APIs.
class FastPairClient {
 public:
  virtual ~FastPairClient() = default;
  // Gets an observed device.
  // Blocking function
  virtual absl::StatusOr<proto::GetObservedDeviceResponse> GetObservedDevice(
      const proto::GetObservedDeviceRequest& request) = 0;

  // Reads the user's devices.
  // Blocking function
  virtual absl::StatusOr<proto::UserReadDevicesResponse> UserReadDevices(
      const proto::UserReadDevicesRequest& request) = 0;

  // Writes a new device to a user's account.
  // Blocking function
  virtual absl::StatusOr<proto::UserWriteDeviceResponse> UserWriteDevice(
      const proto::UserWriteDeviceRequest& request) = 0;

  // Deletes an existing device from a user's account.
  // Blocking function
  virtual absl::StatusOr<proto::UserDeleteDeviceResponse> UserDeleteDevice(
      const proto::UserDeleteDeviceRequest& request) = 0;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_CLIENT_H_
