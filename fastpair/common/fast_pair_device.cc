// Copyright 2022 Google LLC
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

#include "fastpair/common/fast_pair_device.h"

#include <ostream>
#include <string>
#include <utility>

#include "fastpair/common/protocol.h"

namespace nearby {
namespace fastpair {

std::ostream& operator<<(std::ostream& stream, const FastPairDevice& device) {
  stream << "[Device: model_id = " << device.GetModelId()
         << ", ble_address = " << device.GetBleAddress()
         << ", public_address = " << device.GetPublicAddress().value_or("null")
         << ", display_name = " << device.GetDisplayName().value_or("null")
         << ", " << device.GetAccountKey()
         << ", protocol = " << device.GetProtocol() << "]";

  return stream;
}

}  // namespace fastpair
}  // namespace nearby
