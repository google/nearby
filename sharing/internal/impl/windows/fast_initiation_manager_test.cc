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

#include "sharing/internal/impl/windows/fast_initiation_manager.h"

#include <array>
#include <cstdint>

#include "gtest/gtest.h"

namespace nearby::sharing::windows {
namespace {

// The beacon the official client advertises while a send surface is open.
TEST(FastInitBleBeaconTest, SerializesSilentBeaconLikeOfficialClient) {
  FastInitBleBeacon beacon{};
  beacon.SetVersion(FastInitBleBeacon::FastInitVersion::kV1);
  beacon.SetType(FastInitBleBeacon::FastInitType::kSilent);
  beacon.SetAdjustedTxPower(-66);
  beacon.SetRequireBtAdvertising(true);

  beacon.SerializeToByteArray();

  std::array<uint8_t, FastInitBleBeacon::kAdvertiseDataTotalSize> expected{
      0x2c, 0xfe, 0xfc, 0x12, 0x8e, 0x04, 0x42};
  expected[25] = 0x80;
  EXPECT_EQ(beacon.GetAdDataByteArray(), expected);
}

TEST(FastInitBleBeaconTest, SerializesUwbAndSenderCertFields) {
  FastInitBleBeacon beacon{};
  beacon.SetType(FastInitBleBeacon::FastInitType::kNotify);
  beacon.SetUwbSupported(true);
  beacon.SetSenderCertSupported(true);
  beacon.SetUwbMetadata({0x11});
  beacon.SetUwbAddress({1, 2, 3, 4, 5, 6, 7, 8});
  beacon.SetSalt({0x22});
  beacon.SetSecretIdHash({9, 10, 11, 12, 13, 14, 15, 16});
  beacon.SetSelfOnlyAdvertising(true);

  beacon.SerializeToByteArray();

  EXPECT_EQ(beacon.GetAdDataByteArray(),
            (std::array<uint8_t, FastInitBleBeacon::kAdvertiseDataTotalSize>{
                0x2c, 0xfe, 0xfc, 0x12, 0x8e, 0x03, 0x00, 0x11, 1,  2,
                3,    4,    5,    6,    7,    8,    0x22, 9,    10, 11,
                12,   13,   14,   15,   16,   0x40}));
}

}  // namespace
}  // namespace nearby::sharing::windows
