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

#include "fastpair/internal/mediums/bluetooth_classic.h"

#include "gtest/gtest.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(BluetoothClassicTest, CanCreatePairing) {
  MediumEnvironment::Instance().Start();
  BluetoothRadio radio;
  BluetoothClassic bluetooth_classic(radio);
  BluetoothAdapter provider_adapter;
  provider_adapter.SetStatus(BluetoothAdapter::Status::kEnabled);
  BluetoothClassicMedium bt_provider(provider_adapter);

  EXPECT_TRUE(bluetooth_classic.IsAvailable());
  EXPECT_TRUE(
      bluetooth_classic.CreatePairing(provider_adapter.GetMacAddress()));
  MediumEnvironment::Instance().Stop();
}

TEST(BluetoothClassicTest, RemoteDeviceNotFound) {
  MediumEnvironment::Instance().Start();
  BluetoothRadio radio;
  BluetoothClassic bluetooth_classic(radio);
  BluetoothAdapter provider_adapter;
  provider_adapter.SetStatus(BluetoothAdapter::Status::kEnabled);
  BluetoothClassicMedium bt_provider(provider_adapter);

  EXPECT_TRUE(bluetooth_classic.IsAvailable());
  EXPECT_FALSE(bluetooth_classic.CreatePairing("bleaddress"));
  MediumEnvironment::Instance().Stop();
}

TEST(BluetoothClassicTest, RadioDisable) {
  BluetoothRadio radio;
  BluetoothClassic bluetooth_classic(radio);
  BluetoothAdapter provider_adapter;
  provider_adapter.SetStatus(BluetoothAdapter::Status::kEnabled);
  BluetoothClassicMedium bt_provider(provider_adapter);
  radio.Disable();

  EXPECT_FALSE(bluetooth_classic.IsAvailable());
  EXPECT_FALSE(
      bluetooth_classic.CreatePairing(provider_adapter.GetMacAddress()));
}

TEST(BluetoothClassicTest, BluetoothAdapterDisable) {
  BluetoothRadio radio;
  radio.GetBluetoothAdapter().SetStatus(BluetoothAdapter::Status::kDisabled);
  BluetoothClassic bluetooth_classic(radio);
  BluetoothAdapter adapter_provider;
  adapter_provider.SetStatus(BluetoothAdapter::Status::kEnabled);
  BluetoothClassicMedium bt_provider(adapter_provider);

  EXPECT_FALSE(bluetooth_classic.IsAvailable());
  EXPECT_FALSE(
      bluetooth_classic.CreatePairing(adapter_provider.GetMacAddress()));
}

TEST(BluetoothClassicTest, GetMedium) {
  BluetoothRadio radio;
  BluetoothClassic bluetooth_classic(radio);

  EXPECT_TRUE(bluetooth_classic.GetMedium().IsValid());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
