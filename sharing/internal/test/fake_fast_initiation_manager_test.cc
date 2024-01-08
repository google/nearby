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

#include "sharing/internal/test/fake_fast_initiation_manager.h"

#include <functional>
#include <string>

#include "gtest/gtest.h"
#include "sharing/internal/api/fast_init_ble_beacon.h"
#include "sharing/internal/api/fast_initiation_manager.h"

namespace nearby {
namespace {

TEST(FakeFastInitiationManager, StartAdvertising) {
  bool started = false;
  std::function<void()> success_callback = [&started]() { started = true; };
  std::function<void(api::FastInitiationManager::Error)> error_callback =
      [](api::FastInitiationManager::Error) {};

  FakeFastInitiationManager fast_initiation_manager;
  fast_initiation_manager.StartAdvertising(
      api::FastInitBleBeacon::FastInitType::kNotify, success_callback,
      error_callback);
  fast_initiation_manager.SetAdvertisingStarted();

  EXPECT_TRUE(started);
}

TEST(FakeFastInitiationManager, StopAdvertising) {
  bool stopped = false;
  std::function<void()> success_callback = [&stopped]() { stopped = true; };

  FakeFastInitiationManager fast_initiation_manager;
  fast_initiation_manager.StopAdvertising(success_callback);
  fast_initiation_manager.SetAdvertisingStopped();

  EXPECT_TRUE(stopped);
}

TEST(FakeFastInitiationManager, StartScanning) {
  bool devices_discovered = false;
  std::function<void()> devices_discovered_callback = [&devices_discovered]() {
    devices_discovered = true;
  };
  std::function<void()> devices_not_discovered_callback = []() {};
  std::function<void(api::FastInitiationManager::Error)> error_callback =
      [](api::FastInitiationManager::Error) {};

  FakeFastInitiationManager fast_initiation_manager;
  fast_initiation_manager.StartScanning(devices_discovered_callback,
                                        devices_not_discovered_callback,
                                        error_callback);
  fast_initiation_manager.SetScanningDiscovered();

  EXPECT_TRUE(devices_discovered);
}

TEST(FakeFastInitiationManager, StopScanning) {
  bool stopped = false;
  std::function<void()> success_callback = [&stopped]() { stopped = true; };

  FakeFastInitiationManager fast_initiation_manager;
  fast_initiation_manager.StopScanning(success_callback);
  fast_initiation_manager.SetScanningStopped();

  EXPECT_TRUE(stopped);
}

}  // namespace
}  // namespace nearby
