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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PROTO_PROTO_BUILDER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PROTO_PROTO_BUILDER_H_

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/proto/data.pb.h"

namespace nearby {
namespace fastpair {

// Builds FastPairInfo proto from a FastPairDevice instance
void BuildFastPairInfo(::nearby::fastpair::proto::FastPairInfo* fast_pair_info,
                       const FastPairDevice& fast_pair_device);

// Builds FastPairInfo proto from OptInStatus
void BuildFastPairInfo(::nearby::fastpair::proto::FastPairInfo* fast_pair_info,
                       proto::OptInStatus opt_in_status);

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PROTO_PROTO_BUILDER_H_
