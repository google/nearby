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

FastPairDevice::FastPairDevice(std::string model_id, std::string ble_address,
                               Protocol protocol)
    : model_id(std::move(model_id)),
      ble_address(std::move(ble_address)),
      protocol(protocol) {}

FastPairDevice::~FastPairDevice() = default;

std::ostream& operator<<(std::ostream& stream, const FastPairDevice& device) {
  stream << "[Device: model_id = " << device.model_id
         << ", ble_address = " << device.ble_address
         << ", piblic_address = " << device.public_address().value_or("null")
         << ", display_name = " << device.display_name().value_or("null")
         << ", protocol = " << device.protocol << "]";

  return stream;
}

}  // namespace fastpair
}  // namespace nearby
