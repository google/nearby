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
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "fastpair/common/protocol.h"

namespace nearby {
namespace fastpair {

enum class DeviceFastPairVersion {
  kV1,
  kHigherThanV1,
};

// Thin class which is used by the higher level components of the Fast Pair
// system to represent a device.
struct FastPairDevice {
  FastPairDevice(std::string model_id, std::string ble_address,
                 Protocol protocol);
  FastPairDevice(const FastPairDevice&) = delete;
  FastPairDevice& operator=(const FastPairDevice&) = delete;
  FastPairDevice& operator=(FastPairDevice&&) = delete;
  ~FastPairDevice();

  const std::optional<std::string>& public_address() const {
    return public_address_;
  }

  void set_public_address(const std::string& address) {
    public_address_ = address;
  }

  const std::optional<std::string>& display_name() const {
    return display_name_;
  }

  void set_display_name(const std::optional<std::string>& display_name) {
    display_name_ = display_name;
  }

  std::optional<DeviceFastPairVersion> version() { return version_; }

  void set_version(std::optional<DeviceFastPairVersion> version) {
    version_ = version;
  }

  std::optional<std::vector<uint8_t>> account_key() const {
    return account_key_;
  }

  void set_account_key(std::vector<uint8_t> account_key) {
    account_key_ = account_key;
  }

  const std::string model_id;

  // Bluetooth LE address of the device.
  const std::string ble_address;

  // The Quick Pair protocol implementation that this device belongs to.
  const Protocol protocol;

 private:
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
  std::optional<std::vector<uint8_t>> account_key_;
};

std::ostream& operator<<(std::ostream& stream, const FastPairDevice& device);

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_DEVICE_H_
