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
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {
class BluetoothClassicMediumObserver
    : public BluetoothClassicMedium ::Observer {
 public:
  explicit BluetoothClassicMediumObserver(
      BluetoothClassic* bluetooth_classic, CountDownLatch* device_added_latch,
      CountDownLatch* device_removed_latch,
      CountDownLatch* device_paired_changed_latch)
      : bluetooth_classic_(bluetooth_classic),
        device_added_latch_(device_added_latch),
        device_removed_latch_(device_removed_latch),
        device_paired_changed_latch_(device_paired_changed_latch) {
    bluetooth_classic_->AddObserver(this);
  }

  ~BluetoothClassicMediumObserver() override {
    bluetooth_classic_->RemoveObserver(this);
  }

  void DeviceAdded(BluetoothDevice& device) override {
    if (!device_added_latch_) return;
    device_added_latch_->CountDown();
  }

  void DeviceRemoved(BluetoothDevice& device) override {
    if (!device_removed_latch_) return;
    device_removed_latch_->CountDown();
  }

  void DevicePairedChanged(BluetoothDevice& device,
                           bool new_paired_status) override {
    if (!device_paired_changed_latch_) return;
    device_paired_changed_latch_->CountDown();
  }

  BluetoothClassic* bluetooth_classic_;
  CountDownLatch* device_added_latch_;
  CountDownLatch* device_removed_latch_;
  CountDownLatch* device_paired_changed_latch_;
};

TEST(BluetoothClassicTest, CanStartAndStopDiscovery) {
  MediumEnvironment::Instance().Start();
  BluetoothRadio radio;
  BluetoothClassic bluetooth_classic(radio);
  BluetoothAdapter provider_adapter;
  provider_adapter.SetStatus(BluetoothAdapter::Status::kEnabled);
  provider_adapter.SetScanMode(
      BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  BluetoothClassicMedium bt_provider(provider_adapter);
  CountDownLatch device_added_latch(1);
  CountDownLatch device_removed_latch(1);
  BluetoothClassicMediumObserver observer(
      &bluetooth_classic, &device_added_latch, &device_removed_latch, nullptr);

  EXPECT_TRUE(bluetooth_classic.StartDiscovery());
  device_added_latch.Await();
  provider_adapter.SetStatus(BluetoothAdapter::Status::kDisabled);
  device_removed_latch.Await();

  EXPECT_TRUE(bluetooth_classic.StopDiscovery());
  MediumEnvironment::Instance().Stop();
}

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

TEST(BluetoothClassicTest, FailedToCreatePairingDueToRemoteDeviceNotFound) {
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
  EXPECT_FALSE(bluetooth_classic.StartDiscovery());
  EXPECT_FALSE(bluetooth_classic.StopDiscovery());
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
  EXPECT_FALSE(bluetooth_classic.StartDiscovery());
  EXPECT_FALSE(bluetooth_classic.StopDiscovery());
}

TEST(BluetoothClassicTest, GetMedium) {
  BluetoothRadio radio;
  BluetoothClassic bluetooth_classic(radio);

  EXPECT_TRUE(bluetooth_classic.GetMedium().IsValid());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
