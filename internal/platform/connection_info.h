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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CONNECTION_INFO_H_

#include "internal/platform/byte_array.h"

namespace nearby {

constexpr int kMacAddressLength = 6;

class ConnectionInfo {
 public:
  enum class MediumType {
    kUnknown = 0,
    kBluetooth = 1,
    kWifiLan = 2,
    kBle = 3,
  };
  virtual ~ConnectionInfo() = default;
  virtual MediumType GetMediumType() const = 0;
  virtual ByteArray ToBytes() const = 0;
};
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CONNECTION_INFO_H_
