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

#include "fastpair/scanning/scanner_broker_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"
#include "fastpair/scanning/fastpair/fake_fast_pair_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fake_fast_pair_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner_impl.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"
#include "gtest/gtest.h"
#include "internal/platform/implementation/system_clock.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::string_view kTestDeviceAddress("11:12:13:14:15:16");
constexpr absl::string_view kValidModelId("718c17");
constexpr absl::Duration kTaskWaitTimeout = absl::Milliseconds(200);

class FakeFastPairScannerFactory : public FastPairScannerImpl::Factory {
 public:
  // FastPairScannerImpl::Factory:
  std::shared_ptr<FastPairScanner> CreateInstance() override {
    auto fake_fast_pair_scanner = std::shared_ptr<FakeFastPairScanner>();
    fake_fast_pair_scanner_ = fake_fast_pair_scanner.get();
    return fake_fast_pair_scanner;
  }

  ~FakeFastPairScannerFactory() override = default;

  FakeFastPairScanner* fake_fast_pair_scanner() {
    return fake_fast_pair_scanner_;
  }

 private:
  FakeFastPairScanner* fake_fast_pair_scanner_ = nullptr;
};

class FakeFastPairDiscoverableScannerFactory
    : public FastPairDiscoverableScannerImpl::Factory {
 public:
  // FastPairDiscoverableScannerImpl::Factory:
  std::unique_ptr<FastPairDiscoverableScanner> CreateInstance(
      std::shared_ptr<FastPairScanner> scanner,
      std::shared_ptr<BluetoothAdapter> adapter, DeviceCallback found_callback,
      DeviceCallback lost_callback) override {
    create_instance_ = true;
    auto fake_fast_pair_discoverable_scanner =
        std::make_unique<FakeFastPairDiscoverableScanner>(
            std::move(found_callback), std::move(lost_callback));
    fake_fast_pair_discoverable_scanner_ =
        fake_fast_pair_discoverable_scanner.get();
    return fake_fast_pair_discoverable_scanner;
  }

  FakeFastPairDiscoverableScanner* fake_fast_pair_discoverable_scanner() {
    return fake_fast_pair_discoverable_scanner_;
  }

  bool create_instance() { return create_instance_; }

 protected:
  bool create_instance_ = false;
  FakeFastPairDiscoverableScanner* fake_fast_pair_discoverable_scanner_ =
      nullptr;
};

class ScannerBrokerImplTest : public testing::Test,
                              public ScannerBroker::Observer {
 public:
  void SetUp() override {
    adapter_ = std::shared_ptr<BluetoothAdapter>();

    scanner_factory_ = std::make_unique<FakeFastPairScannerFactory>();
    FastPairScannerImpl::Factory::SetFactoryForTesting(scanner_factory_.get());

    discoverable_scanner_factory_ =
        std::make_unique<FakeFastPairDiscoverableScannerFactory>();
    FastPairDiscoverableScannerImpl::Factory::SetFactoryForTesting(
        discoverable_scanner_factory_.get());

    scanner_broker_ = std::make_unique<ScannerBrokerImpl>();
    scanner_broker_->AddObserver(this);
  }

  void TearDown() override {
    scanner_broker_->RemoveObserver(this);
    scanner_broker_.reset();
    scanner_factory_.reset();
    discoverable_scanner_factory_.reset();
    FastPairScannerImpl::Factory::SetFactoryForTesting(nullptr);
    FastPairDiscoverableScannerImpl::Factory::SetFactoryForTesting(nullptr);
    adapter_.reset();
  }

  void TriggerDiscoverableDeviceFound() {
    FastPairDevice device(std::string(kValidModelId),
                          std::string(kTestDeviceAddress),
                          Protocol::kFastPairInitialPairing);
    discoverable_scanner_factory_->fake_fast_pair_discoverable_scanner()
        ->TriggerDeviceFoundCallback(device);
  }

  void TriggerDiscoverableDeviceLost() {
    FastPairDevice device(std::string(kValidModelId),
                          std::string(kTestDeviceAddress),
                          Protocol::kFastPairInitialPairing);
    discoverable_scanner_factory_->fake_fast_pair_discoverable_scanner()
        ->TriggerDeviceLostCallback(device);
  }

  void OnDeviceFound(const FastPairDevice& device) override {
    device_found_ = true;
  }

  void OnDeviceLost(const FastPairDevice& device) override {
    device_lost_ = true;
  }

 protected:
  bool device_found_ = false;
  bool device_lost_ = false;
  std::shared_ptr<BluetoothAdapter> adapter_;
  std::unique_ptr<FakeFastPairScannerFactory> scanner_factory_;
  std::unique_ptr<FakeFastPairDiscoverableScannerFactory>
      discoverable_scanner_factory_;
  std::unique_ptr<ScannerBroker> scanner_broker_;
};

TEST_F(ScannerBrokerImplTest, DiscoverableFound) {
  EXPECT_FALSE(discoverable_scanner_factory_->create_instance());

  scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
  SystemClock::Sleep(kTaskWaitTimeout);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(discoverable_scanner_factory_->create_instance());

  TriggerDiscoverableDeviceFound();
  EXPECT_TRUE(device_found_);
}

TEST_F(ScannerBrokerImplTest, DiscoverableLost) {
  EXPECT_FALSE(discoverable_scanner_factory_->create_instance());

  scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
  SystemClock::Sleep(kTaskWaitTimeout);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(discoverable_scanner_factory_->create_instance());

  TriggerDiscoverableDeviceLost();
  EXPECT_TRUE(device_lost_);
}

TEST_F(ScannerBrokerImplTest, RemoveObserver) {
  EXPECT_FALSE(discoverable_scanner_factory_->create_instance());

  scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
  SystemClock::Sleep(kTaskWaitTimeout);
  EXPECT_FALSE(device_found_);
  EXPECT_TRUE(discoverable_scanner_factory_->create_instance());

  scanner_broker_->RemoveObserver(this);
  TriggerDiscoverableDeviceLost();
  EXPECT_FALSE(device_lost_);
}

TEST_F(ScannerBrokerImplTest, StopScanning) {
  EXPECT_FALSE(discoverable_scanner_factory_->create_instance());

  scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
  SystemClock::Sleep(kTaskWaitTimeout);
  EXPECT_TRUE(discoverable_scanner_factory_->create_instance());

  scanner_broker_->StopScanning(Protocol::kFastPairInitialPairing);
  SystemClock::Sleep(kTaskWaitTimeout);

  scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
  SystemClock::Sleep(kTaskWaitTimeout);
  EXPECT_TRUE(discoverable_scanner_factory_->create_instance());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
