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

#include "fastpair/proto/proto_builder.h"

#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/enum.proto.h"
#include "fastpair/proto/fast_pair_string.proto.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr char kModelId[] = "9adb11";
constexpr char kBleAddress[] = "11::22::33::44::55::66";
constexpr char kPublicAddress[] = "20:64:DE:40:F8:93";
constexpr char kDisplayName[] = "Test Device";
constexpr char kInitialPairingdescription[] = "InitialPairingdescription";
constexpr char kAccountKey[] = "04b85786180add47fb81a04a8ce6b0de";
constexpr char kExpectedSha256Hash[] =
    "6353c0075a35b7d81bb30a6190ab246da4b8c55a6111d387400579133c090ed8";
TEST(ProtoBuilderTest, BuildFastPairInfo) {
  // Creates a FastPaireDevice instance.
  FastPairDevice device(kModelId, kBleAddress,
                        Protocol::kFastPairInitialPairing);
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));
  device.SetAccountKey(account_key);
  device.SetPublicAddress(kPublicAddress);
  device.SetDisplayName(kDisplayName);
  proto::GetObservedDeviceResponse response;
  auto* observed_device_strings = response.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata(response);
  device.SetMetadata(device_metadata);

  // Builds FastPairInfo from the created FastPairDevice
  proto::FastPairInfo fast_proto_info;
  BuildFastPairInfo(&fast_proto_info, device);

  EXPECT_EQ(fast_proto_info.device().account_key(), account_key.GetAsBytes());
  EXPECT_EQ(absl::BytesToHexString(
                fast_proto_info.device().sha256_account_key_public_address()),
            kExpectedSha256Hash);
  proto::StoredDiscoveryItem stored_discovery_item;
  EXPECT_TRUE(stored_discovery_item.ParseFromString(
      fast_proto_info.device().discovery_item_bytes()));
  EXPECT_EQ(stored_discovery_item.title(), kDisplayName);
  proto::FastPairStrings fast_pair_strings =
      stored_discovery_item.fast_pair_strings();
  EXPECT_EQ(fast_pair_strings.initial_pairing_description(),
            kInitialPairingdescription);
}

TEST(ProtoBuilderTest, BuildFastPairInfoForOptIn) {
  proto::FastPairInfo fast_proto_info;
  BuildFastPairInfo(&fast_proto_info,
                    proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);
  EXPECT_EQ(fast_proto_info.opt_in_status(),
            proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
