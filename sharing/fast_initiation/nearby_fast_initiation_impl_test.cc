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

#include "sharing/fast_initiation/nearby_fast_initiation_impl.h"

#include <functional>
#include <memory>

#include "gtest/gtest.h"
#include "sharing/fast_initiation/fake_nearby_fast_initiation_observer.h"
#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_fast_initiation_manager.h"

namespace nearby {
namespace sharing {
namespace {

TEST(NearbyFastInitiationImpl, IsLowEnergySupported) {
  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  EXPECT_TRUE(nearby_fast_initiation_impl.IsLowEnergySupported());
}

TEST(NearbyFastInitiationImpl, IsScanOffloadSupported) {
  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  EXPECT_TRUE(nearby_fast_initiation_impl.IsScanOffloadSupported());
}

TEST(NearbyFastInitiationImpl, IsAdvertisementOffloadSupported) {
  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  EXPECT_TRUE(nearby_fast_initiation_impl.IsAdvertisementOffloadSupported());
}

TEST(NearbyFastInitiationImpl, HasObserverReturnsFalseAfterRemovingObserver) {
  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  FakeNearbyFastInitiationObserver fake_observer(&nearby_fast_initiation_impl);

  nearby_fast_initiation_impl.AddObserver(&fake_observer);
  EXPECT_TRUE(nearby_fast_initiation_impl.HasObserver(&fake_observer));

  nearby_fast_initiation_impl.RemoveObserver(&fake_observer);
  EXPECT_FALSE(nearby_fast_initiation_impl.HasObserver(&fake_observer));
}

TEST(NearbyFastInitiationImpl, HasObserver) {
  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  FakeNearbyFastInitiationObserver fake_observer(&nearby_fast_initiation_impl);

  FakeNearbyFastInitiationObserver fake_observer_1(
      &nearby_fast_initiation_impl);
  FakeNearbyFastInitiationObserver fake_observer_2(
      &nearby_fast_initiation_impl);

  nearby_fast_initiation_impl.AddObserver(&fake_observer_1);

  EXPECT_TRUE(nearby_fast_initiation_impl.HasObserver(&fake_observer_1));
  EXPECT_FALSE(nearby_fast_initiation_impl.HasObserver(&fake_observer_2));
}

TEST(NearbyFastInitiationImpl, StartAdvertising) {
  bool success_callback_called = false;
  std::function<void()> success_callback = [&]() {
    success_callback_called = true;
  };
  std::function<void()> error_callback = []() {};

  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  nearby_fast_initiation_impl.StartAdvertising(
      NearbyFastInitiation::FastInitType::kNotify, success_callback,
      error_callback);
  FakeFastInitiationManager& fake_fast_initiation_manager =
      *fake_context->fake_fast_initiation_manager();
  fake_fast_initiation_manager.SetAdvertisingStarted();
  EXPECT_TRUE(success_callback_called);
}

TEST(NearbyFastInitiationImpl, StartAdvertisingAndGetHardwareError) {
  bool error_callback_called = false;
  std::function<void()> success_callback = []() {};
  std::function<void()> error_callback = [&]() {
    error_callback_called = true;
  };

  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());

  FakeNearbyFastInitiationObserver fake_observer(&nearby_fast_initiation_impl);

  nearby_fast_initiation_impl.AddObserver(&fake_observer);

  nearby_fast_initiation_impl.StartAdvertising(
      NearbyFastInitiation::FastInitType::kNotify, success_callback,
      error_callback);

  FakeFastInitiationManager& fake_fast_initiation_manager =
      *fake_context->fake_fast_initiation_manager();

  // Mocking OS hardware error event on Windows
  fake_fast_initiation_manager.SetAdvertisingError(
      nearby::api::FastInitiationManager::Error::kBluetoothRadioUnavailable);

  EXPECT_TRUE(error_callback_called);
  EXPECT_EQ(fake_observer.GetNumHardwareErrorReported(), 1);
}

TEST(NearbyFastInitiationImpl, StopAdvertisingNotStart) {
  bool success_callback_called = false;
  std::function<void()> success_callback = [&]() {
    success_callback_called = true;
  };

  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  nearby_fast_initiation_impl.StopAdvertising(success_callback);
  EXPECT_TRUE(success_callback_called);
}

TEST(NearbyFastInitiationImpl, StopStartedAdvertising) {
  bool start_callback_called = false;
  bool stop_callback_called = false;
  auto fake_context = std::make_unique<FakeContext>();
  FakeFastInitiationManager& fake_fast_initiation_manager =
      *fake_context->fake_fast_initiation_manager();

  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  nearby_fast_initiation_impl.StartAdvertising(
      NearbyFastInitiation::FastInitType::kNotify,
      [&]() { start_callback_called = true; }, [&]() {});
  fake_fast_initiation_manager.SetAdvertisingStarted();
  EXPECT_TRUE(start_callback_called);
  EXPECT_TRUE(nearby_fast_initiation_impl.IsAdvertising());
  nearby_fast_initiation_impl.StopAdvertising(
      [&]() { stop_callback_called = true; });

  fake_fast_initiation_manager.SetAdvertisingStopped();
  EXPECT_TRUE(stop_callback_called);
}

TEST(NearbyFastInitiationImpl, StartScanningSucceed) {
  bool devices_discovered = false;
  std::function<void()> devices_discovered_callback = [&devices_discovered]() {
    devices_discovered = true;
  };
  std::function<void()> devices_not_discovered_callback = []() {};
  std::function<void()> error_callback = []() {};

  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  nearby_fast_initiation_impl.StartScanning(devices_discovered_callback,
                                            devices_not_discovered_callback,
                                            error_callback);
  FakeFastInitiationManager& fake_fast_initiation_manager =
      *fake_context->fake_fast_initiation_manager();
  fake_fast_initiation_manager.SetScanningDiscovered();
  EXPECT_TRUE(devices_discovered);
}

TEST(NearbyFastInitiationImpl, StartScanningAndGetHardwareError) {
  bool error_callback_called = false;
  std::function<void()> devices_discovered_callback = []() {};
  std::function<void()> devices_not_discovered_callback = []() {};
  std::function<void()> error_callback = [&]() {
    error_callback_called = true;
  };

  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());

  FakeNearbyFastInitiationObserver fake_observer(&nearby_fast_initiation_impl);

  nearby_fast_initiation_impl.AddObserver(&fake_observer);

  nearby_fast_initiation_impl.StartScanning(devices_discovered_callback,
                                            devices_not_discovered_callback,
                                            error_callback);

  FakeFastInitiationManager& fake_fast_initiation_manager =
      *fake_context->fake_fast_initiation_manager();

  // Mocking OS hardware error event on Windows
  fake_fast_initiation_manager.SetScanningError(
      nearby::api::FastInitiationManager::Error::kBluetoothRadioUnavailable);

  EXPECT_TRUE(error_callback_called);
  EXPECT_EQ(fake_observer.GetNumHardwareErrorReported(), 1);
}

TEST(NearbyFastInitiationImpl, StopScanningNotStart) {
  bool stop_callback_called = false;
  std::function<void()> success_callback = [&]() {
    stop_callback_called = true;
  };

  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  nearby_fast_initiation_impl.StopScanning(success_callback);
  EXPECT_TRUE(stop_callback_called);
}

TEST(NearbyFastInitiationImpl, StopStartedScanning) {
  bool stop_callback_called = false;
  auto fake_context = std::make_unique<FakeContext>();
  NearbyFastInitiationImpl nearby_fast_initiation_impl(fake_context.get());
  FakeFastInitiationManager& fake_fast_initiation_manager =
      *fake_context->fake_fast_initiation_manager();
  nearby_fast_initiation_impl.StartScanning([]() {}, []() {}, []() {});
  nearby_fast_initiation_impl.StopScanning(
      [&]() { stop_callback_called = true; });
  fake_fast_initiation_manager.SetScanningStopped();
  EXPECT_TRUE(stop_callback_called);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
