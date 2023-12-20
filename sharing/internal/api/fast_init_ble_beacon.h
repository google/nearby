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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAST_INIT_BLE_BEACON_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAST_INIT_BLE_BEACON_H_

#include <array>
#include <cstdint>

namespace nearby {
namespace api {

class FastInitBleBeacon {
 public:
  // Possible types of error raised while registering or unregistering Fast
  // Initiation BLE Beacon.
  enum class ErrorCode : int {
    kUnsupportedPlatform = 0,  // BLE Beacon not supported
                               // on current platform.
    kBeaconAlreadyExists,      // A BLE Beacon is already
                               // registered.
    kBeaconDoesNotExist,       // Unregistering a BLE Beacon which
                               // is not registered.
    kBeaconInvalidLength,      // BLE Beacon is not of a valid
                               // length.
    kFailToStartBeacon,        // Error when starting the BLE Beacon
                               // scanning/advertising through a platform API.
    kFailToStopBeacon,         // Error when stopping the BLE Beacon
                               // scanning/advertising through a platform API.
    kFailToResetBeacon,        // Error while resetting BLE Beacon.
    kAdapterPoweredOff,        // Error because the adapter is off
    kUnknown
  };

  enum class FastInitVersion : int { kV1 = 0 };

  enum class FastInitType : int { kNotify = 0, kSilent = 1 };

  static constexpr uint8_t kFastInitServiceUuid[] = {0xfe, 0x2c};
  static constexpr uint8_t kFastInitModelId[] = {0xfc, 0x12, 0x8e};

  // Size of fields in AdvertisingData Service Data (in bytes)
  static constexpr uint8_t kFastInitServiceUuidSize = 2;
  static constexpr uint8_t kFastInitModelIdSize = 3;
  // FastInit V1 Metadata (2 bytes)
  // (1 byte) [ version (3 bits) | type (3 bits) | uwb_supported (1 bit) |
  // sender_cert_supported (1 bit)]
  // (1 byte) [ adjusted_tx_power]
  static constexpr uint8_t kUwbMetadataSize = 1;
  static constexpr uint8_t kUwbAddressSize = 8;
  static constexpr uint8_t kSaltSize = 1;
  static constexpr uint8_t kSecretIdHashSize = 8;

  // require_bt_advertising (1 bit) | self_only_advertising (1 bit) |
  // unused (6 bits)
  static constexpr uint8_t kRequireBtAdvertising = 1;
  static constexpr uint8_t kSelfOnlyAdvertising = 1;

  static constexpr uint8_t kAdvertiseDataTotalSize = 26;

  virtual ~FastInitBleBeacon() = default;

  FastInitVersion GetVersion() const { return version_; }
  FastInitType GetType() const { return type_; }
  bool GetUwbSupported() const { return is_uwb_supported_; }
  bool GetSenderCertSupported() const { return is_sender_cert_supported_; }
  int8_t GetAdjustedTxPower() const { return adjusted_tx_power_; }
  std::array<uint8_t, kUwbMetadataSize> GetUwbMetadata() const {
    return uwb_metadata_;
  }
  std::array<uint8_t, kUwbAddressSize> GetUwbAddress() const {
    return uwb_address_;
  }
  std::array<uint8_t, kSaltSize> GetSalt() const { return salt_; }
  std::array<uint8_t, kSecretIdHashSize> GetSecretIdHash() const {
    return secret_id_hash_;
  }
  bool GetRequireBtAdvertising() const { return require_bt_advertising_; }
  bool GetSelfOnlyAdvertising() const { return self_only_advertising_; }

  void SetVersion(FastInitVersion version) { version_ = version; }
  void SetType(FastInitType type) { type_ = type; }
  void SetUwbSupported(bool is_supported) { is_uwb_supported_ = is_supported; }
  void SetSenderCertSupported(bool is_supported) {
    is_sender_cert_supported_ = is_supported;
  }
  void SetAdjustedTxPower(int8_t signed_value) {
    adjusted_tx_power_ = signed_value;
  }
  void SetUwbMetadata(std::array<uint8_t, kUwbMetadataSize> byte_array) {
    uwb_metadata_ = byte_array;
  }
  void SetUwbAddress(std::array<uint8_t, kUwbAddressSize> byte_array) {
    uwb_address_ = byte_array;
  }
  void SetSalt(std::array<uint8_t, kSaltSize> byte_array) {
    salt_ = byte_array;
  }
  void SetSecretIdHash(std::array<uint8_t, kSecretIdHashSize> byte_array) {
    secret_id_hash_ = byte_array;
  }

  std::array<uint8_t, kAdvertiseDataTotalSize> GetAdDataByteArray() {
    return advertising_data_byte_array_;
  }
  void SetAdDataByteArray(
      std::array<uint8_t, kAdvertiseDataTotalSize> byte_array) {
    advertising_data_byte_array_ = byte_array;
  }

  void SetRequireBtAdvertising(bool require_bt_advertising) {
    require_bt_advertising_ = require_bt_advertising;
  }
  void SetSelfOnlyAdvertising(bool self_only_advertising) {
    self_only_advertising_ = self_only_advertising;
  }

  virtual void SerializeToByteArray() = 0;
  virtual void ParseFromByteArray() = 0;

 protected:
  FastInitVersion version_;
  FastInitType type_;
  bool is_uwb_supported_;
  bool is_sender_cert_supported_;
  int8_t adjusted_tx_power_;
  std::array<uint8_t, kUwbMetadataSize> uwb_metadata_;
  std::array<uint8_t, kUwbAddressSize> uwb_address_;
  std::array<uint8_t, kSaltSize> salt_;
  std::array<uint8_t, kSecretIdHashSize> secret_id_hash_;
  bool require_bt_advertising_;
  bool self_only_advertising_;

  std::array<uint8_t, kAdvertiseDataTotalSize> advertising_data_byte_array_;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAST_INIT_BLE_BEACON_H_
