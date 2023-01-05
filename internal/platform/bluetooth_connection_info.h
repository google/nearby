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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLUETOOTH_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLUETOOTH_CONNECTION_INFO_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/connection_info.h"

namespace nearby {

class BluetoothConnectionInfo : public ConnectionInfo {
 public:
  MediumType GetMediumType() const override { return MediumType::kBluetooth; }
  ByteArray ToBytes() const override;
  BluetoothConnectionInfo() = delete;
  explicit BluetoothConnectionInfo(const ByteArray& mac_address,
                                   absl::string_view service_id)
      : mac_address_(mac_address.data()), service_id_(std::string(service_id)) {
    mac_address_.resize(kMacAddressLength);
  }

  BluetoothConnectionInfo(BluetoothConnectionInfo const& info) {
    mac_address_ = info.mac_address_;
    service_id_ = info.service_id_;
  }
  static BluetoothConnectionInfo FromBytes(ByteArray bytes);
  ByteArray GetMacAddress() const { return ByteArray(mac_address_); }
  absl::string_view GetServiceId() const { return service_id_; }

 private:
  std::string mac_address_;
  std::string service_id_;
};

inline bool operator==(const BluetoothConnectionInfo& a,
                       const BluetoothConnectionInfo& b) {
  return a.GetMacAddress() == b.GetMacAddress() &&
         a.GetServiceId() == b.GetServiceId();
}

inline bool operator!=(const BluetoothConnectionInfo& a,
                       const BluetoothConnectionInfo& b) {
  return !(a == b);
}

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BLUETOOTH_CONNECTION_INFO_H_
