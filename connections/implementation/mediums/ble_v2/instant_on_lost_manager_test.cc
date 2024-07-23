// Copyright 2024 Google LLC
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

#include "connections/implementation/mediums/ble_v2/instant_on_lost_manager.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr absl::string_view kServiceIdA = "A";
constexpr absl::string_view kServiceIdB = "B";
constexpr absl::string_view kServiceIdC = "C";
constexpr absl::string_view kData1 = "\x01\x02\x03";
constexpr absl::string_view kData2 = "\x04\x05\x06";
constexpr absl::string_view kData3 = "\x07\x08\x09";
constexpr absl::string_view kData4 = "\x10\x11\x12";
constexpr absl::string_view kData5 = "\x13\x14\x15";
constexpr absl::string_view kData6 = "\x16\x17\x18";

constexpr absl::Duration kOnLostAdvertisingDuration = absl::Milliseconds(2100);

TEST(InstantOnLostManager, StartOnLostAdvertisingAfterStopAdvertising) {
  InstantOnLostManager instant_on_lost_manager;
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  absl::SleepFor(kOnLostAdvertisingDuration);
  EXPECT_FALSE(instant_on_lost_manager.IsOnLostAdvertising());
}

TEST(InstantOnLostManager, NoOnLostAdvertisingWhenAdvertiseAgain) {
  InstantOnLostManager instant_on_lost_manager;
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  EXPECT_FALSE(instant_on_lost_manager.IsOnLostAdvertising());
  instant_on_lost_manager.Shutdown();
}

TEST(InstantOnLostManager, MultipleAdvertisingOnSameServiceId) {
  InstantOnLostManager instant_on_lost_manager;
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  EXPECT_EQ(instant_on_lost_manager.GetOnLostHashes().size(), 1);
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData2.data(), kData2.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  EXPECT_EQ(instant_on_lost_manager.GetOnLostHashes().size(), 2);
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData3.data(), kData3.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  EXPECT_EQ(instant_on_lost_manager.GetOnLostHashes().size(), 3);
  instant_on_lost_manager.Shutdown();
}

TEST(InstantOnLostManager, MaximumOnLostHashesInOnLostAdvertising) {
  InstantOnLostManager instant_on_lost_manager;
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData2.data(), kData2.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData3.data(), kData3.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdB), ByteArray(kData4.data(), kData4.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdB));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdB), ByteArray(kData5.data(), kData5.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdB));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdC), ByteArray(kData6.data(), kData6.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdC));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  EXPECT_EQ(instant_on_lost_manager.GetOnLostHashes().size(), 5);
  instant_on_lost_manager.Shutdown();
}

TEST(InstantOnLostManager, ShutdownMultipleTimes) {
  InstantOnLostManager instant_on_lost_manager;
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  EXPECT_TRUE(instant_on_lost_manager.Shutdown());
  EXPECT_FALSE(instant_on_lost_manager.Shutdown());
}

TEST(InstantOnLostManager, AdvertisingOnShutdownManager) {
  InstantOnLostManager instant_on_lost_manager;
  instant_on_lost_manager.Shutdown();
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_FALSE(instant_on_lost_manager.IsOnLostAdvertising());
}

TEST(InstantOnLostManager, RemoveExpiredOnLostAdvertisement) {
  InstantOnLostManager instant_on_lost_manager;
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  EXPECT_EQ(instant_on_lost_manager.GetOnLostHashes().size(), 1);
  absl::SleepFor(absl::Milliseconds(1050));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData2.data(), kData2.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  EXPECT_EQ(instant_on_lost_manager.GetOnLostHashes().size(), 2);
  absl::SleepFor(absl::Milliseconds(1050));
  instant_on_lost_manager.OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData3.data(), kData3.size()));
  instant_on_lost_manager.OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager.IsOnLostAdvertising());
  EXPECT_EQ(instant_on_lost_manager.GetOnLostHashes().size(), 2);
  instant_on_lost_manager.Shutdown();
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
