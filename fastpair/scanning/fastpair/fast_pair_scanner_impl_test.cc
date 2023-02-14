// Copyright 2022 Google LLC

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fastpair/common/constant.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kTestModelId[] = "112233";

class FakeBlePeripheral : public api::BlePeripheral {
 public:
  explicit FakeBlePeripheral(absl::string_view name,
                             absl::string_view service_data) {
    name_ = std::string(name);
    std::string service_data_str = std::string(service_data);
    ByteArray advertisement_bytes(service_data_str);
    advertisement_data_ = advertisement_bytes;
  }
  FakeBlePeripheral(const FakeBlePeripheral&) = default;
  ~FakeBlePeripheral() override = default;

  std::string GetName() const override { return name_; }

  ByteArray GetAdvertisementBytes(
      const std::string& service_id) const override {
    return advertisement_data_;
  }

  void SetName(const std::string& name) { name_ = name; }

  void SetAdvertisementBytes(ByteArray advertisement_bytes) {
    advertisement_data_ = advertisement_bytes;
  }

 private:
  std::string name_;
  ByteArray advertisement_data_;
};

class FastPairScannerObserver : public FastPairScanner::Observer {
 public:
  // FastPairScanner::Observer overrides
  void OnDeviceFound(const BlePeripheral& peripheral) override {
    device_addreses_.push_back(peripheral.GetName());
    on_device_found_count_++;
  }

  void OnDeviceLost(const BlePeripheral& peripheral) override {
    auto it = std::find(device_addreses_.begin(), device_addreses_.end(),
                        peripheral.GetName());
    if (it == device_addreses_.end()) return;
    device_addreses_.erase(it);
  }

  bool DoesDeviceListContainTestDevice(const std::string& address) {
    auto it =
        std::find(device_addreses_.begin(), device_addreses_.end(), address);
    return it != device_addreses_.end();
  }

  int on_device_found_count() { return on_device_found_count_; }

 private:
  std::vector<std::string> device_addreses_;

  int on_device_found_count_ = 0;
};

class FastPairScannerImplTest : public testing::Test {
 public:
  void SetUp() override {
    scanner_.reset();
    scanner_ = std::make_shared<FastPairScannerImpl>();
    scanner_observer_ = std::make_unique<FastPairScannerObserver>();
    scanner_->AddObserver(scanner_observer_.get());
  }

  void TearDown() override { env_.Stop(); }

  void TriggerOnDeviceFound(absl::string_view address, absl::string_view data) {
    auto ble_peripheral = std::make_unique<FakeBlePeripheral>(address, data);
    scanner_->OnDeviceFound(BlePeripheral(ble_peripheral.get()));
  }

  void TriggerOnDeviceLost(absl::string_view address, absl::string_view data) {
    auto ble_peripheral = std::make_unique<FakeBlePeripheral>(address, data);
    scanner_->OnDeviceLost(BlePeripheral(ble_peripheral.get()));
  }

 protected:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
  std::shared_ptr<FastPairScannerImpl> scanner_;
  std::unique_ptr<FastPairScannerObserver> scanner_observer_;
};

TEST_F(FastPairScannerImplTest, FactoryCreatSuccessfully) {
  env_.Start();
  std::shared_ptr<FastPairScanner> scanner =
      FastPairScannerImpl::Factory::Create();
  EXPECT_TRUE(scanner);
  scanner.reset();
  env_.Stop();
}

TEST_F(FastPairScannerImplTest, StartScanningSuccessfully) {
  env_.Start();
  scanner_->StartScanning();
  SystemClock::Sleep(absl::Milliseconds(200));
  EXPECT_TRUE(scanner_->GetBle().IsScanning());
  // Not StopScanning as FastPairLowPowerDisabled
  env_.Stop();
}

TEST_F(FastPairScannerImplTest, DeviceFoundNotifiesObservers) {
  env_.Start();
  TriggerOnDeviceFound(kTestBleDeviceAddress, kTestModelId);
  EXPECT_TRUE(scanner_observer_->DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress));
  env_.Stop();
}

TEST_F(FastPairScannerImplTest, DeviceLostNotifiesObservers) {
  env_.Start();
  TriggerOnDeviceFound(kTestBleDeviceAddress, kTestModelId);
  EXPECT_TRUE(scanner_observer_->DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress));
  TriggerOnDeviceLost(kTestBleDeviceAddress, kTestModelId);
  EXPECT_FALSE(scanner_observer_->DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress));
  env_.Stop();
}

TEST_F(FastPairScannerImplTest, DeviceFoundWithNoServiceData) {
  env_.Start();
  TriggerOnDeviceFound(kTestBleDeviceAddress, "");
  EXPECT_FALSE(scanner_observer_->DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress));
  env_.Stop();
}

TEST_F(FastPairScannerImplTest, RemoveObserver) {
  env_.Start();
  scanner_->RemoveObserver(scanner_observer_.get());
  TriggerOnDeviceFound(kTestBleDeviceAddress, kTestModelId);
  EXPECT_FALSE(scanner_observer_->DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress));
  env_.Stop();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
