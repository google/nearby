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

#include "sharing/internal/test/fake_bluetooth_adapter.h"

#include <stdint.h>

#include <array>
#include <functional>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/test/fake_bluetooth_adapter_observer.h"

namespace nearby {
namespace {

TEST(FakeBluetoothAdapter, IsPresent) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_TRUE(fake_bluetooth_adapter.IsPresent());
}

TEST(FakeBluetoothAdapter, IsPowered) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_TRUE(fake_bluetooth_adapter.IsPowered());
}

TEST(FakeBluetoothAdapter, IsLowEnergySupported) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_TRUE(fake_bluetooth_adapter.IsLowEnergySupported());
}

TEST(FakeBluetoothAdapter, IsScanOffloadSupported) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_TRUE(fake_bluetooth_adapter.IsScanOffloadSupported());
}

TEST(FakeBluetoothAdapter, IsAdvertisementOffloadSupported) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_TRUE(fake_bluetooth_adapter.IsAdvertisementOffloadSupported());
}

TEST(FakeBluetoothAdapter, IsExtendedAdvertisingSupported) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_TRUE(fake_bluetooth_adapter.IsExtendedAdvertisingSupported());
}

TEST(FakeBluetoothAdapter, IsPeripheralRoleSupported) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_TRUE(fake_bluetooth_adapter.IsPeripheralRoleSupported());
}

TEST(FakeBluetoothAdapter, GetOSPermissionStatus) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_EQ(fake_bluetooth_adapter.GetOsPermissionStatus(),
            sharing::api::BluetoothAdapter::PermissionStatus::kAllowed);
}

TEST(FakeBluetoothAdapter, SetPowered) {
  bool powered_on;
  std::function<void()> success_callback = [&powered_on]() {
    powered_on = true;
  };
  std::function<void()> error_callback = [&powered_on]() {
    powered_on = false;
  };
  FakeBluetoothAdapter fake_bluetooth_adapter;
  fake_bluetooth_adapter.SetPowered(true, success_callback, error_callback);
  EXPECT_TRUE(powered_on);
}

TEST(FakeBluetoothAdapter, GetAdapterId) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  EXPECT_EQ(fake_bluetooth_adapter.GetAdapterId(), "nearby");
}

TEST(FakeBluetoothAdapter, GetAddress) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  fake_bluetooth_adapter.SetAddress("1a:1b:1c:1d:1e:1f");
  // Expected conversion from "1a:1b:1c:1d:1e:1f"
  std::array<uint8_t, 6> expected_output{{26, 27, 28, 29, 30, 31}};
  EXPECT_EQ(fake_bluetooth_adapter.GetAddress(), expected_output);
}

TEST(FakeBluetoothAdapter, AddObserver) {
  FakeBluetoothAdapter fake_bluetooth_adapter;
  FakeBluetoothAdapterObserver fake_observer(&fake_bluetooth_adapter);
  fake_bluetooth_adapter.AddObserver(&fake_observer);
  EXPECT_TRUE(fake_bluetooth_adapter.HasObserver(&fake_observer));
}

TEST(FakeBluetoothAdapter, RemoveObserver) {
  FakeBluetoothAdapter fake_bluetooth_adapter;

  FakeBluetoothAdapterObserver fake_observer_1(&fake_bluetooth_adapter);
  FakeBluetoothAdapterObserver fake_observer_2(&fake_bluetooth_adapter);

  fake_bluetooth_adapter.AddObserver(&fake_observer_1);
  fake_bluetooth_adapter.AddObserver(&fake_observer_2);

  EXPECT_TRUE(fake_bluetooth_adapter.HasObserver(&fake_observer_1));
  EXPECT_TRUE(fake_bluetooth_adapter.HasObserver(&fake_observer_2));

  fake_bluetooth_adapter.RemoveObserver(&fake_observer_1);

  EXPECT_FALSE(fake_bluetooth_adapter.HasObserver(&fake_observer_1));
  EXPECT_TRUE(fake_bluetooth_adapter.HasObserver(&fake_observer_2));
}

TEST(FakeBluetoothAdapter, HasObserver) {
  FakeBluetoothAdapter fake_bluetooth_adapter_1;
  FakeBluetoothAdapter fake_bluetooth_adapter_2;

  FakeBluetoothAdapterObserver fake_observer_1(&fake_bluetooth_adapter_1);
  FakeBluetoothAdapterObserver fake_observer_2(&fake_bluetooth_adapter_2);

  fake_bluetooth_adapter_1.AddObserver(&fake_observer_1);
  fake_bluetooth_adapter_2.AddObserver(&fake_observer_2);

  EXPECT_TRUE(fake_bluetooth_adapter_1.HasObserver(&fake_observer_1));
  EXPECT_FALSE(fake_bluetooth_adapter_1.HasObserver(&fake_observer_2));

  EXPECT_TRUE(fake_bluetooth_adapter_2.HasObserver(&fake_observer_2));
  EXPECT_FALSE(fake_bluetooth_adapter_2.HasObserver(&fake_observer_1));
}

TEST(FakeBluetoothAdapter, AdapterPresentChanged) {
  FakeBluetoothAdapter fake_bluetooth_adapter_1;
  FakeBluetoothAdapter fake_bluetooth_adapter_2;

  FakeBluetoothAdapterObserver fake_observer_1a(&fake_bluetooth_adapter_1);
  FakeBluetoothAdapterObserver fake_observer_1b(&fake_bluetooth_adapter_1);
  FakeBluetoothAdapterObserver fake_observer_2(&fake_bluetooth_adapter_2);

  fake_bluetooth_adapter_1.AddObserver(&fake_observer_1a);
  fake_bluetooth_adapter_1.AddObserver(&fake_observer_1b);
  fake_bluetooth_adapter_2.AddObserver(&fake_observer_2);

  // Mocking OS adapter presence changed events (enabled -> disabled/unplugged)
  fake_bluetooth_adapter_1.ReceivedAdapterPresentChangedFromOs(false);

  EXPECT_FALSE(fake_observer_1a.GetObservedPresentValue());
  EXPECT_FALSE(fake_observer_1b.GetObservedPresentValue());
  EXPECT_TRUE(fake_observer_2.GetObservedPresentValue());
}

TEST(FakeBluetoothAdapter, AdapterPoweredChanged) {
  FakeBluetoothAdapter fake_bluetooth_adapter_1;
  FakeBluetoothAdapter fake_bluetooth_adapter_2;

  FakeBluetoothAdapterObserver fake_observer_1a(&fake_bluetooth_adapter_1);
  FakeBluetoothAdapterObserver fake_observer_1b(&fake_bluetooth_adapter_1);
  FakeBluetoothAdapterObserver fake_observer_2(&fake_bluetooth_adapter_2);

  fake_bluetooth_adapter_1.AddObserver(&fake_observer_1a);
  fake_bluetooth_adapter_1.AddObserver(&fake_observer_1b);
  fake_bluetooth_adapter_2.AddObserver(&fake_observer_2);

  // Mocking OS adapter powered changed events (on -> off)
  fake_bluetooth_adapter_1.ReceivedAdapterPoweredChangedFromOs(false);

  EXPECT_FALSE(fake_observer_1a.GetObservedPoweredValue());
  EXPECT_FALSE(fake_observer_1b.GetObservedPoweredValue());
  EXPECT_TRUE(fake_observer_2.GetObservedPoweredValue());
}

TEST(FakeBluetoothAdapter, RepeatedAdapterPresentChanged) {
  FakeBluetoothAdapter fake_bluetooth_adapter;

  FakeBluetoothAdapterObserver fake_observer_1(&fake_bluetooth_adapter);
  FakeBluetoothAdapterObserver fake_observer_2(&fake_bluetooth_adapter);

  fake_bluetooth_adapter.AddObserver(&fake_observer_1);
  fake_bluetooth_adapter.AddObserver(&fake_observer_2);

  // Mocking first OS adapter present changed event (enabled ->
  // disabled/unplugged)
  fake_bluetooth_adapter.ReceivedAdapterPresentChangedFromOs(false);

  EXPECT_EQ(fake_bluetooth_adapter.GetNumPresentReceivedFromOS(), 1);
  EXPECT_EQ(fake_observer_1.GetNumAdapterPresentChanged(), 1);
  EXPECT_EQ(fake_observer_2.GetNumAdapterPresentChanged(), 1);

  // Mocking second OS adapter present changed event (enabled ->
  // disabled/unplugged)
  fake_bluetooth_adapter.ReceivedAdapterPresentChangedFromOs(false);

  // Since it is a repeated event, do not inform observers
  // i.e. observers have still only updated the state change once
  EXPECT_EQ(fake_bluetooth_adapter.GetNumPresentReceivedFromOS(), 2);
  EXPECT_EQ(fake_observer_1.GetNumAdapterPresentChanged(), 1);
  EXPECT_EQ(fake_observer_2.GetNumAdapterPresentChanged(), 1);

  EXPECT_FALSE(fake_observer_1.GetObservedPresentValue());
  EXPECT_FALSE(fake_observer_2.GetObservedPresentValue());
}

TEST(FakeBluetoothAdapter, RepeatedAdapterPoweredChanged) {
  FakeBluetoothAdapter fake_bluetooth_adapter;

  FakeBluetoothAdapterObserver fake_observer_1(&fake_bluetooth_adapter);
  FakeBluetoothAdapterObserver fake_observer_2(&fake_bluetooth_adapter);

  fake_bluetooth_adapter.AddObserver(&fake_observer_1);
  fake_bluetooth_adapter.AddObserver(&fake_observer_2);

  // Mocking first OS adapter powered changed event (on -> off)
  fake_bluetooth_adapter.ReceivedAdapterPoweredChangedFromOs(false);

  EXPECT_EQ(fake_bluetooth_adapter.GetNumPoweredReceivedFromOS(), 1);
  EXPECT_EQ(fake_observer_1.GetNumAdapterPoweredChanged(), 1);
  EXPECT_EQ(fake_observer_2.GetNumAdapterPoweredChanged(), 1);

  // Mocking second OS adapter powered changed event (on -> off)
  fake_bluetooth_adapter.ReceivedAdapterPoweredChangedFromOs(false);

  // Since it is a repeated event, do not inform observers
  // i.e. observers have still only updated the state change once
  EXPECT_EQ(fake_bluetooth_adapter.GetNumPoweredReceivedFromOS(), 2);
  EXPECT_EQ(fake_observer_1.GetNumAdapterPoweredChanged(), 1);
  EXPECT_EQ(fake_observer_2.GetNumAdapterPoweredChanged(), 1);

  EXPECT_FALSE(fake_observer_1.GetObservedPoweredValue());
  EXPECT_FALSE(fake_observer_2.GetObservedPoweredValue());
}

}  // namespace
}  // namespace nearby
