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

#include "connections/implementation/mediums/ble/instant_on_lost_manager.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/medium_environment.h"

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

class InstantOnLostManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MediumEnvironment::Instance().SetBleExtendedAdvertisementsAvailable(true);
    instant_on_lost_manager_ = std::make_unique<InstantOnLostManager>();
  }

  void TearDown() override { instant_on_lost_manager_->Shutdown(); }

  InstantOnLostManager& instant_on_lost_manager() {
    return *instant_on_lost_manager_;
  }

 private:
  std::unique_ptr<InstantOnLostManager> instant_on_lost_manager_;
};

TEST_F(InstantOnLostManagerTest, StartOnLostAdvertisingAfterStopAdvertising) {
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  absl::SleepFor(kOnLostAdvertisingDuration);
  EXPECT_FALSE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
}

TEST_F(InstantOnLostManagerTest, NoOnLostAdvertisingWhenAdvertiseAgain) {
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  EXPECT_FALSE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
}

TEST_F(InstantOnLostManagerTest, MultipleAdvertisingOnSameServiceId) {
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  EXPECT_EQ(instant_on_lost_manager().GetOnLostHashesForTesting().size(), 1);
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData2.data(), kData2.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  EXPECT_EQ(instant_on_lost_manager().GetOnLostHashesForTesting().size(), 2);
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData3.data(), kData3.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  EXPECT_EQ(instant_on_lost_manager().GetOnLostHashesForTesting().size(), 3);
}

TEST_F(InstantOnLostManagerTest, MaximumOnLostHashesInOnLostAdvertising) {
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData2.data(), kData2.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData3.data(), kData3.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdB), ByteArray(kData4.data(), kData4.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdB));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdB), ByteArray(kData5.data(), kData5.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdB));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdC), ByteArray(kData6.data(), kData6.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdC));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  EXPECT_EQ(instant_on_lost_manager().GetOnLostHashesForTesting().size(), 5);
}

TEST_F(InstantOnLostManagerTest, ShutdownMultipleTimes) {
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  EXPECT_TRUE(instant_on_lost_manager().Shutdown());
  EXPECT_FALSE(instant_on_lost_manager().Shutdown());
}

TEST_F(InstantOnLostManagerTest, AdvertisingOnShutdownManager) {
  instant_on_lost_manager().Shutdown();
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_FALSE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
}

TEST_F(InstantOnLostManagerTest, RemoveExpiredOnLostAdvertisement) {
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData1.data(), kData1.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  EXPECT_EQ(instant_on_lost_manager().GetOnLostHashesForTesting().size(), 1);
  absl::SleepFor(absl::Milliseconds(1050));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData2.data(), kData2.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  EXPECT_EQ(instant_on_lost_manager().GetOnLostHashesForTesting().size(), 2);
  absl::SleepFor(absl::Milliseconds(1050));
  instant_on_lost_manager().OnAdvertisingStarted(
      std::string(kServiceIdA), ByteArray(kData3.data(), kData3.size()));
  instant_on_lost_manager().OnAdvertisingStopped(std::string(kServiceIdA));
  EXPECT_TRUE(instant_on_lost_manager().IsOnLostAdvertisingForTesting());
  EXPECT_EQ(instant_on_lost_manager().GetOnLostHashesForTesting().size(), 2);
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
