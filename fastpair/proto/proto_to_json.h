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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PROTO_PROTO_TO_JSON_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PROTO_PROTO_TO_JSON_H_

#include "nlohmann/json_fwd.hpp"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"

namespace nearby {
namespace fastpair {
// Converts Fast Pair protos to readable, JSON-style dictionaries.

// GetObservedDeviceRequest/Response
nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::GetObservedDeviceRequest& request);
nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::GetObservedDeviceResponse& response);

// UserReadDevicesRequest/Response
nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::UserReadDevicesRequest& request);
nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::UserReadDevicesResponse& response);

// UserWriteDeviceRequest
nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::UserWriteDeviceRequest& request);

// UserDeleteDeviceRequest/Response
nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::UserDeleteDeviceRequest& request);
nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::UserDeleteDeviceResponse& response);

nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::FastPairInfo& fast_pair_info);

nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::FastPairDevice& fast_pair_device);

nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::ObservedDeviceStrings& strings);

nlohmann::json FastPairProtoToJson(
    const nearby::fastpair::proto::Device& device);
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PROTO_PROTO_TO_JSON_H_
