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

#include "internal/platform/wifi_utils.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"

namespace nearby {
namespace {

constexpr int kChan6Num_2G = 9;
constexpr int kChan6NumFreq_2G = 2452;

constexpr int kChan48Num_5G = 48;
constexpr int kChan48NumFreq_5G = 5240;

constexpr int kChan69Num_6G = 69;
constexpr int kChan69NumFreq_6G = 6295;

constexpr int kChan4Num_60G = 4;
constexpr int kChan4NumFreq_60G = 64800;

constexpr int kChan20Num_2G_NotExist = 20;
constexpr int kChan180Num_5G_NotExist = 180;
constexpr int kChan0Num_6G_NotExist = 0;
constexpr int kChan30Num_60G_NotExist = 30;

constexpr int kFreqNotExist = 1002;

TEST(WifiUtilsTest, ConvertChannelToFrequency) {
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan6Num_2G,
                                                    WifiBandType::kUnknown),
            kChan6NumFreq_2G);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan6Num_2G,
                                                    WifiBandType::kBand24Ghz),
            kChan6NumFreq_2G);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan48Num_5G,
                                                    WifiBandType::kBand5Ghz),
            kChan48NumFreq_5G);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan69Num_6G,
                                                    WifiBandType::kBand6Ghz),
            kChan69NumFreq_6G);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan4Num_60G,
                                                    WifiBandType::kBand60Ghz),
            kChan4NumFreq_60G);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan20Num_2G_NotExist,
                                                    WifiBandType::kBand24Ghz),
            WifiUtils::kUnspecified);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan180Num_5G_NotExist,
                                                    WifiBandType::kBand5Ghz),
            WifiUtils::kUnspecified);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan0Num_6G_NotExist,
                                                    WifiBandType::kBand6Ghz),
            WifiUtils::kUnspecified);
  EXPECT_EQ(WifiUtils::ConvertChannelToFrequencyMhz(kChan30Num_60G_NotExist,
                                                    WifiBandType::kBand60Ghz),
            WifiUtils::kUnspecified);
}

TEST(WifiUtilsTest, ConvertFrequencyToChannel) {
  EXPECT_EQ(WifiUtils::ConvertFrequencyMhzToChannel(kChan6NumFreq_2G),
            kChan6Num_2G);
  EXPECT_EQ(WifiUtils::ConvertFrequencyMhzToChannel(kChan48NumFreq_5G),
            kChan48Num_5G);
  EXPECT_EQ(WifiUtils::ConvertFrequencyMhzToChannel(kChan69NumFreq_6G),
            kChan69Num_6G);
  EXPECT_EQ(WifiUtils::ConvertFrequencyMhzToChannel(kChan4NumFreq_60G),
            kChan4Num_60G);
  EXPECT_EQ(WifiUtils::ConvertFrequencyMhzToChannel(kFreqNotExist),
            WifiUtils::kUnspecified);
}

TEST(WifiUtilsTest, Ipv4Validation) {
  EXPECT_FALSE(WifiUtils::ValidateIPV4("12.34.212.46.37"));
  EXPECT_FALSE(WifiUtils::ValidateIPV4("12.gt.212.46"));
  EXPECT_FALSE(WifiUtils::ValidateIPV4("192.168.358.46"));
  EXPECT_FALSE(WifiUtils::ValidateIPV4("192.-168.1.46"));
  EXPECT_TRUE(WifiUtils::ValidateIPV4("192.168.1.46"));
}

TEST(WifiUtilsTest, GetHumanReadableIpAddress) {
  EXPECT_EQ(
      WifiUtils::GetHumanReadableIpAddress(absl::HexStringToBytes("000AFEFF")),
      "0.10.254.255");
}

}  // namespace
}  // namespace nearby
