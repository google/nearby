// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_PCP_H_
#define CORE_INTERNAL_PCP_H_

namespace nearby {
namespace connections {

// The PreConnectionProtocol (PCP) defines the combinations of interactions
// between the techniques (ultrasound audio, Bluetooth device names, BLE
// advertisements) used for offline Advertisement + Discovery, and identifies
// the steps to go through on each device.
//
// See go/nearby-offline-data-interchange-formats for more.
enum class Pcp {
  kUnknown = 0,
  kP2pStar = 1,
  kP2pCluster = 2,
  kP2pPointToPoint = 3,
  // PCP is only allocated 5 bits in our data interchange formats, so there can
  // never be more than 31 PCP values.
};
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_PCP_H_
