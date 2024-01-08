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

#include "sharing/internal/test/fake_wifi_adapter.h"

#include <functional>
#include <string>

#include "gtest/gtest.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/test/fake_wifi_adapter_observer.h"

namespace nearby {
namespace {

TEST(FakeWifiAdapter, IsPresentReturnsTrueByDefault) {
  FakeWifiAdapter fake_wifi_adapter;
  EXPECT_TRUE(fake_wifi_adapter.IsPresent());
}

TEST(FakeWifiAdapter, IsPoweredReturnsTrueByDefault) {
  FakeWifiAdapter fake_wifi_adapter;
  EXPECT_TRUE(fake_wifi_adapter.IsPowered());
}

TEST(FakeWifiAdapter, GetOSPermissionStatusReturnsAllowedByDefault) {
  FakeWifiAdapter fake_wifi_adapter;
  EXPECT_EQ(fake_wifi_adapter.GetOsPermissionStatus(),
            sharing::api::WifiAdapter::PermissionStatus::kAllowed);
}

TEST(FakeWifiAdapter, SetPoweredRunsSuccessCallback) {
  bool powered_on;
  std::function<void()> success_callback = [&powered_on]() {
    powered_on = true;
  };
  std::function<void()> error_callback = [&powered_on]() {
    powered_on = false;
  };
  FakeWifiAdapter fake_wifi_adapter;
  fake_wifi_adapter.SetPowered(/*powered=*/true, success_callback,
                               error_callback);
  EXPECT_TRUE(powered_on);
}

TEST(FakeWifiAdapter, GetAdapterIdReturnsNearbyByDefault) {
  FakeWifiAdapter fake_wifi_adapter;
  EXPECT_EQ(fake_wifi_adapter.GetAdapterId(), "nearby");
}

TEST(FakeWifiAdapter, HasObserverReturnsTrueAfterAddingObserver) {
  FakeWifiAdapter fake_wifi_adapter;
  FakeWifiAdapterObserver fake_observer(&fake_wifi_adapter);
  fake_wifi_adapter.AddObserver(&fake_observer);
  EXPECT_TRUE(fake_wifi_adapter.HasObserver(&fake_observer));
}

TEST(FakeWifiAdapter, HasObserverReturnsFalseAfterRemovingObserver) {
  FakeWifiAdapter fake_wifi_adapter;
  FakeWifiAdapterObserver fake_observer(&fake_wifi_adapter);

  fake_wifi_adapter.AddObserver(&fake_observer);
  EXPECT_TRUE(fake_wifi_adapter.HasObserver(&fake_observer));

  fake_wifi_adapter.RemoveObserver(&fake_observer);
  EXPECT_FALSE(fake_wifi_adapter.HasObserver(&fake_observer));
}

TEST(FakeWifiAdapter, HasObserver) {
  FakeWifiAdapter fake_wifi_adapter;

  FakeWifiAdapterObserver fake_observer_1(&fake_wifi_adapter);
  FakeWifiAdapterObserver fake_observer_2(&fake_wifi_adapter);

  fake_wifi_adapter.AddObserver(&fake_observer_1);

  EXPECT_TRUE(fake_wifi_adapter.HasObserver(&fake_observer_1));
  EXPECT_FALSE(fake_wifi_adapter.HasObserver(&fake_observer_2));
}

TEST(FakeWifiAdapter, AdapterPresentChanged) {
  FakeWifiAdapter fake_wifi_adapter;
  FakeWifiAdapterObserver fake_observer(&fake_wifi_adapter);
  fake_wifi_adapter.AddObserver(&fake_observer);

  // Mocking OS adapter presence changed events (enabled -> disabled/unplugged)
  fake_wifi_adapter.ReceivedAdapterPresentChangedFromOs(/*present=*/false);
  EXPECT_FALSE(fake_observer.GetObservedPresentValue());
}

TEST(FakeWifiAdapter, AdapterPoweredChanged) {
  FakeWifiAdapter fake_wifi_adapter;
  FakeWifiAdapterObserver fake_observer(&fake_wifi_adapter);

  fake_wifi_adapter.AddObserver(&fake_observer);

  // Mocking OS adapter powered changed events (on -> off)
  fake_wifi_adapter.ReceivedAdapterPoweredChangedFromOs(/*powered=*/false);
  EXPECT_FALSE(fake_observer.GetObservedPoweredValue());
}

TEST(FakeWifiAdapter, RepeatedAdapterPresentChanged) {
  FakeWifiAdapter fake_wifi_adapter;
  FakeWifiAdapterObserver fake_observer(&fake_wifi_adapter);

  fake_wifi_adapter.AddObserver(&fake_observer);

  // Mocking first OS adapter present changed event (enabled ->
  // disabled/unplugged)
  fake_wifi_adapter.ReceivedAdapterPresentChangedFromOs(/*present=*/false);

  EXPECT_EQ(fake_wifi_adapter.GetNumPresentReceivedFromOS(), 1);
  EXPECT_EQ(fake_observer.GetNumAdapterPresentChanged(), 1);

  // Mocking second OS adapter present changed event (enabled ->
  // disabled/unplugged)
  fake_wifi_adapter.ReceivedAdapterPresentChangedFromOs(/*present=*/false);

  // Since it is a repeated event, do not inform observers
  // i.e. observers have still only updated the state change once
  EXPECT_EQ(fake_wifi_adapter.GetNumPresentReceivedFromOS(), 2);
  EXPECT_EQ(fake_observer.GetNumAdapterPresentChanged(), 1);

  EXPECT_FALSE(fake_observer.GetObservedPresentValue());
}

TEST(FakeWifiAdapter, RepeatedAdapterPoweredChanged) {
  FakeWifiAdapter fake_wifi_adapter;
  FakeWifiAdapterObserver fake_observer(&fake_wifi_adapter);

  fake_wifi_adapter.AddObserver(&fake_observer);

  // Mocking first OS adapter powered changed event (on -> off)
  fake_wifi_adapter.ReceivedAdapterPoweredChangedFromOs(/*powered=*/false);

  EXPECT_EQ(fake_wifi_adapter.GetNumPoweredReceivedFromOS(), 1);
  EXPECT_EQ(fake_observer.GetNumAdapterPoweredChanged(), 1);

  // Mocking second OS adapter powered changed event (on -> off)
  fake_wifi_adapter.ReceivedAdapterPoweredChangedFromOs(/*powered=*/false);

  // Since it is a repeated event, do not inform observers
  // i.e. observers have still only updated the state change once
  EXPECT_EQ(fake_wifi_adapter.GetNumPoweredReceivedFromOS(), 2);
  EXPECT_EQ(fake_observer.GetNumAdapterPoweredChanged(), 1);

  EXPECT_FALSE(fake_observer.GetObservedPoweredValue());
}

}  // namespace
}  // namespace nearby
