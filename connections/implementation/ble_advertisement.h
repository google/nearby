// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_BLE_ADVERTISEMENT_H_
#define CORE_INTERNAL_BLE_ADVERTISEMENT_H_

#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/pcp.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {

// Represents the format of the Connections Ble Advertisement used in
// Advertising + Discovery.
//
// <p>[VERSION][PCP][SERVICE_ID_HASH][ENDPOINT_ID][ENDPOINT_INFO_SIZE]
//    [ENDPOINT_INFO][BLUETOOTH_MAC][UWB_ADDRESS_SIZE][UWB_ADDRESS][EXTRA_FIELD]
//
// <p>The fast version of this advertisement simply omits SERVICE_ID_HASH and
//    the Bluetooth MAC address.
//
// <p>See go/connections-ble-advertisement for more information.
class BleAdvertisement {
 public:
  // Versions of the BleAdvertisement.
  enum class Version {
    kUndefined = 0,
    kV1 = 1,
    // Version is only allocated 3 bits in the BleAdvertisement, so this
    // can never go beyond V7.
  };

  static constexpr int kVersionAndPcpLength = 1;
  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kPcpBitmask = 0x01F;
  static constexpr int kServiceIdHashLength = 3;
  static constexpr int kEndpointIdLength = 4;
  static constexpr int kEndpointInfoSizeLength = 1;
  static constexpr int kBluetoothMacAddressLength =
      BluetoothUtils::kBluetoothMacAddressLength;
  static constexpr int kUwbAddressSizeLength = 1;
  static constexpr int kExtraFieldLength = 1;
  static constexpr int kEndpointInfoLengthBitmask = 0x0FF;
  static constexpr int kWebRtcConnectableFlagBitmask = 0x01;
  static constexpr int kMinAdvertisementLength =
      kVersionAndPcpLength + kServiceIdHashLength + kEndpointIdLength +
      kEndpointInfoSizeLength + kBluetoothMacAddressLength;

  // The difference between normal and fast advertisements is that the fast one
  // omits the SERVICE_ID_HASH and Bluetooth MAC address. This is done to save
  // space.
  static constexpr int kMinFastAdvertisementLength = kMinAdvertisementLength -
                                                     kServiceIdHashLength -
                                                     kBluetoothMacAddressLength;
  static constexpr int kMaxEndpointInfoLength = 131;
  static constexpr int kMaxFastEndpointInfoLength = 17;

  BleAdvertisement() = default;
  BleAdvertisement(Version version, Pcp pcp, const std::string& endpoint_id,
                   const ByteArray& endpoint_info,
                   const ByteArray& uwb_address);
  BleAdvertisement(Version version, Pcp pcp, const ByteArray& service_id_hash,
                   const std::string& endpoint_id,
                   const ByteArray& endpoint_info,
                   const std::string& bluetooth_mac_address,
                   const ByteArray& uwb_address, WebRtcState web_rtc_state);
  BleAdvertisement(bool fast_advertisement,
                   const ByteArray& ble_advertisement_bytes);
  BleAdvertisement(const BleAdvertisement&) = default;
  BleAdvertisement& operator=(const BleAdvertisement&) = default;
  BleAdvertisement(BleAdvertisement&&) = default;
  BleAdvertisement& operator=(BleAdvertisement&&) = default;
  ~BleAdvertisement() = default;

  explicit operator ByteArray() const;

  bool IsValid() const { return !endpoint_id_.empty(); }
  bool IsFastAdvertisement() const { return fast_advertisement_; }
  Version GetVersion() const { return version_; }
  Pcp GetPcp() const { return pcp_; }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  std::string GetEndpointId() const { return endpoint_id_; }
  ByteArray GetEndpointInfo() const { return endpoint_info_; }
  std::string GetBluetoothMacAddress() const { return bluetooth_mac_address_; }
  ByteArray GetUwbAddress() const { return uwb_address_; }
  WebRtcState GetWebRtcState() const { return web_rtc_state_; }

 private:
  void DoInitialize(bool fast_advertisement, Version version, Pcp pcp,
                    const ByteArray& service_id_hash,
                    const std::string& endpoint_id,
                    const ByteArray& endpoint_info,
                    const std::string& bluetooth_mac_address,
                    const ByteArray& uwb_address, WebRtcState web_rtc_state);

  bool fast_advertisement_ = false;
  Version version_{Version::kUndefined};
  Pcp pcp_{Pcp::kUnknown};
  ByteArray service_id_hash_;
  std::string endpoint_id_;
  ByteArray endpoint_info_;
  std::string bluetooth_mac_address_;
  // TODO(b/169550050): Define UWB address field.
  ByteArray uwb_address_;
  WebRtcState web_rtc_state_{WebRtcState::kUndefined};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BLE_ADVERTISEMENT_H_
