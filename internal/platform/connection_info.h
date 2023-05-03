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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CONNECTION_INFO_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
inline constexpr uint8_t kDataElementFieldType = 0x14;
inline constexpr uint8_t kBluetoothMediumType = 0x00;
inline constexpr uint8_t kBleGattMediumType = 0x01;
inline constexpr uint8_t kWifiLanMediumType = 0x03;
inline constexpr int kMacAddressLength = 6;
inline constexpr int kConnectionInfoMinimumLength = 2;

class BleConnectionInfo;
class BluetoothConnectionInfo;
class WifiLanConnectionInfo;

using ConnectionInfoVariant =
    absl::variant<absl::monostate, BleConnectionInfo, BluetoothConnectionInfo,
                  WifiLanConnectionInfo>;

class ConnectionInfo {
 public:
  virtual ~ConnectionInfo() = default;
  virtual ::location::nearby::proto::connections::Medium GetMediumType()
      const = 0;
  virtual std::string ToDataElementBytes() const = 0;
  virtual std::vector<uint8_t> GetActions() const = 0;
  static ConnectionInfoVariant FromDataElementBytes(
      absl::string_view data_element_bytes);
};
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_CONNECTION_INFO_H_
