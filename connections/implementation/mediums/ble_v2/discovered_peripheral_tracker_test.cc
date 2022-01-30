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

#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

constexpr absl::string_view kFastAdvertisementServiceUuid =
    "0000FE2C-0000-1000-8000-00805F9B34FB";
constexpr absl::string_view kServiceIdA = "A";

class DiscoveredPeripheralTrackerTest : public testing::Test {
 public:
  void SetUp() override {}

 protected:
  DiscoveredPeripheralTracker discovered_peripheral_tracker_;
};

TEST_F(DiscoveredPeripheralTrackerTest, CanStartStopTracking) {
  discovered_peripheral_tracker_.StartTracking(
      std::string(kServiceIdA),
      DiscoveredPeripheralCallback{},
      std::string(kFastAdvertisementServiceUuid));

  discovered_peripheral_tracker_.StopTracking(std::string(kServiceIdA));
}

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
