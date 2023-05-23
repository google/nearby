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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_DEVICE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_DEVICE_H_

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/protocol.h"

namespace nearby {
namespace fastpair {

enum class DeviceFastPairVersion {
  kV1,
  kHigherThanV1,
};

// Thin class which is used by the higher level components of the Fast Pair
// system to represent a device.
class FastPairDevice {
 public:
  explicit FastPairDevice(Protocol protocol) : protocol_(protocol) {}
  FastPairDevice(absl::string_view model_id, absl::string_view ble_address,
                 Protocol protocol)
      : model_id_(model_id), ble_address_(ble_address), protocol_(protocol) {}

  FastPairDevice(const FastPairDevice&) = delete;
  FastPairDevice(FastPairDevice&&) = default;
  FastPairDevice& operator=(const FastPairDevice&) = delete;
  FastPairDevice& operator=(FastPairDevice&&) = default;
  ~FastPairDevice() = default;

  std::optional<std::string> GetPublicAddress() const {
    return public_address_;
  }

  void SetPublicAddress(absl::string_view address) {
    public_address_ = std::string(address);
  }

  std::optional<std::string> GetDisplayName() const { return display_name_; }

  void SetDisplayName(absl::string_view display_name) {
    display_name_ = std::string(display_name);
  }

  std::optional<DeviceFastPairVersion> GetVersion() { return version_; }

  void SetVersion(std::optional<DeviceFastPairVersion> version) {
    version_ = version;
  }

  const AccountKey& GetAccountKey() const { return account_key_; }

  void SetAccountKey(AccountKey account_key) { account_key_ = account_key; }

  void SetModelId(absl::string_view model_id) {
    model_id_ = std::string(model_id);
  }

  absl::string_view GetModelId() const { return model_id_; }

  void SetBleAddress(absl::string_view address) {
    ble_address_ = std::string(address);
  }

  absl::string_view GetBleAddress() const { return ble_address_; }

  Protocol GetProtocol() const { return protocol_; }

  const std::string& GetUniqueId() const {
    if (public_address_.has_value()) {
      return *public_address_;
    }
    return ble_address_;
  }

 private:
  std::string model_id_;

  // Bluetooth LE address of the device.
  std::string ble_address_;

  // The Quick Pair protocol implementation that this device belongs to.
  Protocol protocol_;

  // Bluetooth public classic address of the device.
  std::optional<std::string> public_address_;

  // Display name for the device
  // Similar to Bluetooth classic address field, this will be null when a
  // device is found from a discoverable advertisement due to the fact that
  // initial pair notifications show the OEM default name from the device
  // metadata instead of the display name.
  std::optional<std::string> display_name_;

  // Fast Pair version number, possible versions numbers are defined at the top
  // of this file.
  std::optional<DeviceFastPairVersion> version_;

  // Account key which will be saved to the user's account during Fast Pairing
  // for eligible devices (V2 or higher) and used for detecting subsequent
  // pairing scenarios.
  AccountKey account_key_;
};

std::ostream& operator<<(std::ostream& stream, const FastPairDevice& device);

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_DEVICE_H_
