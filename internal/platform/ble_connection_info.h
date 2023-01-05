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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLE_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLE_CONNECTION_INFO_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/connection_info.h"
#include "internal/platform/logging.h"

namespace nearby {

// 6 bytes that spell "BADMAC"
constexpr absl::string_view kDefunctMacAddr = "\x42\x41\x44\x4D\x41\x43";

class BleConnectionInfo : public ConnectionInfo {
 public:
  explicit BleConnectionInfo(absl::string_view mac_address)
      : mac_address_(std::string(mac_address)) {
    if (mac_address_.size() != kMacAddressLength) {
      NEARBY_LOGS(WARNING)
          << "MAC address is not of the expected length! Trying to "
             "connect to this MAC address will not work!";
      mac_address_ = std::string(kDefunctMacAddr);
    }
  }

  BleConnectionInfo(BleConnectionInfo const& info) {
    mac_address_ = info.mac_address_;
  }
  MediumType GetMediumType() const override { return MediumType::kBle; }
  ByteArray ToBytes() const override;
  static BleConnectionInfo FromBytes(ByteArray bytes);
  ByteArray GetMacAddress() const { return ByteArray(mac_address_); }

 private:
  std::string mac_address_;
};

inline bool operator==(const BleConnectionInfo& a, const BleConnectionInfo& b) {
  return a.GetMacAddress() == b.GetMacAddress();
}

inline bool operator!=(const BleConnectionInfo& a, const BleConnectionInfo& b) {
  return !(a == b);
}

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLE_CONNECTION_INFO_H_
