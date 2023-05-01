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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLUETOOTH_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLUETOOTH_CONNECTION_INFO_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"
#include "proto/connections_enums.pb.h"

namespace nearby {

class BluetoothConnectionInfo : public ConnectionInfo {
 public:
  static absl::StatusOr<BluetoothConnectionInfo> FromDataElementBytes(
      absl::string_view bytes);

  BluetoothConnectionInfo(absl::string_view mac_address,
                          absl::string_view bluetooth_uuid,
                          std::vector<uint8_t> actions)
      : mac_address_(std::string(mac_address)),
        bluetooth_uuid_(std::string(bluetooth_uuid)),
        actions_(actions) {}

  ::location::nearby::proto::connections::Medium GetMediumType()
      const override {
    return ::location::nearby::proto::connections::Medium::BLUETOOTH;
  }
  std::string ToDataElementBytes() const override;
  std::string GetMacAddress() const { return mac_address_; }
  std::string GetBluetoothUuid() const { return bluetooth_uuid_; }
  std::vector<uint8_t> GetActions() const override { return actions_; }

 private:
  std::string mac_address_;
  std::string bluetooth_uuid_;
  std::vector<uint8_t> actions_;
};

inline bool operator==(const BluetoothConnectionInfo& a,
                       const BluetoothConnectionInfo& b) {
  return a.GetMacAddress() == b.GetMacAddress() &&
         a.GetBluetoothUuid() == b.GetBluetoothUuid() &&
         a.GetActions() == b.GetActions();
}

inline bool operator!=(const BluetoothConnectionInfo& a,
                       const BluetoothConnectionInfo& b) {
  return !(a == b);
}

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLUETOOTH_CONNECTION_INFO_H_
