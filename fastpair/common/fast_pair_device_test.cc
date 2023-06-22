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

#include "fastpair/common/fast_pair_device.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/protocol.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(FastPairDevice, GetAndSetAccountKey) {
  FastPairDevice device("model_id", "ble_address",
                        Protocol::kFastPairInitialPairing);
  AccountKey firstKey = AccountKey::CreateRandomKey();
  device.SetAccountKey(firstKey);
  EXPECT_EQ(device.GetAccountKey(), firstKey);

  // Test that overriding works.
  AccountKey secondKey = AccountKey::CreateRandomKey();
  device.SetAccountKey(secondKey);
  EXPECT_EQ(device.GetAccountKey(), secondKey);
}

TEST(FastPairDevice, GetAndSetName) {
  FastPairDevice device("model_id", "ble_address",
                        Protocol::kFastPairInitialPairing);
  // Test that name returns null before any sets.
  EXPECT_FALSE(device.GetDisplayName().has_value());

  // Test that name returns the set value.
  std::string test_name = "test_name";
  device.SetDisplayName(test_name);
  EXPECT_EQ(device.GetDisplayName().value(), test_name);

  // Test that overriding works.
  std::string new_test_name = "new_test_name";
  device.SetDisplayName(new_test_name);
  EXPECT_EQ(device.GetDisplayName().value(), new_test_name);
}

TEST(FastPairDevice, GetAndPublicAddress) {
  FastPairDevice device("model_id", "ble_address",
                        Protocol::kFastPairInitialPairing);
  // Test that public address returns null before any sets.
  EXPECT_FALSE(device.GetPublicAddress().has_value());

  // Test that name returns the set value.
  std::string test_GetPublicAddress = "test_GetPublicAddress ";
  device.SetPublicAddress(test_GetPublicAddress);
  EXPECT_EQ(device.GetPublicAddress().value(), test_GetPublicAddress);

  // Test that overriding works.
  std::string new_test_GetPublicAddress = "new_test_GetPublicAddress ";
  device.SetPublicAddress(new_test_GetPublicAddress);
  EXPECT_EQ(device.GetPublicAddress().value(), new_test_GetPublicAddress);
}

TEST(FastPairDevice, GetVersionUnset) {
  FastPairDevice device("model_id", "ble_address",
                        Protocol::kFastPairInitialPairing);

  EXPECT_FALSE(device.GetVersion());
}

TEST(FastPairDevice, GetVersionV1) {
  FastPairDevice device("model_id", "ble_address",
                        Protocol::kFastPairInitialPairing);
  proto::GetObservedDeviceResponse response;
  device.SetMetadata(DeviceMetadata(response));

  EXPECT_EQ(device.GetVersion(), DeviceFastPairVersion::kV1);
}

TEST(FastPairDevice, GetVersionHigherThanV1) {
  FastPairDevice device("model_id", "ble_address",
                        Protocol::kFastPairInitialPairing);
  proto::GetObservedDeviceResponse response;
  std::string anti_spoofing_key(kPublicKeyByteSize, 0);
  response.mutable_device()->mutable_anti_spoofing_key_pair()->set_public_key(
      anti_spoofing_key);
  device.SetMetadata(DeviceMetadata(response));

  EXPECT_EQ(device.GetVersion(), DeviceFastPairVersion::kHigherThanV1);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
