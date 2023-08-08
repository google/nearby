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

#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::Duration kTaskWaitTimeout = absl::Milliseconds(1000);
constexpr absl::Duration kShortTimeout = absl::Milliseconds(100);
constexpr absl::string_view kServiceID{"Fast Pair"};
constexpr absl::string_view kModelId{"718c17"};
constexpr absl::string_view kFastPairServiceUuid{
    "0000FE2C-0000-1000-8000-00805F9B34FB"};

class FastPairScannerObserver : public FastPairScanner::Observer {
 public:
  explicit FastPairScannerObserver(FastPairScanner* scanner,
                                   CountDownLatch* accept_latch,
                                   CountDownLatch* lost_latch) {
    accept_latch_ = accept_latch;
    lost_latch_ = lost_latch;
    scanner->AddObserver(this);
  }
  // FastPairScanner::Observer overrides
  void OnDeviceFound(const BlePeripheral& peripheral) override {
    accept_latch_->CountDown();
  }

  void OnDeviceLost(const BlePeripheral& peripheral) override {
    lost_latch_->CountDown();
  }

  CountDownLatch* accept_latch_ = nullptr;
  CountDownLatch* lost_latch_ = nullptr;
};

class FastPairScannerImplTest : public testing::Test {
 public:
  void SetUp() override { MediumEnvironment::Instance().Start(); }
  void TearDown() override { MediumEnvironment::Instance().Stop(); }
};

TEST_F(FastPairScannerImplTest, StartScanning) {
  // Create Fast Pair Scanner and add its observer
  Mediums mediums_1;
  SingleThreadExecutor executor;
  auto scanner = std::make_unique<FastPairScannerImpl>(mediums_1, &executor);
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);
  FastPairScannerObserver observer(scanner.get(), &accept_latch, &lost_latch);

  // Create Advertiser and startAdvertising
  Mediums mediums_2;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  // Fast Pair scanner startScanning
  auto scan_session = scanner->StartScanning();
  // Notify device found
  EXPECT_TRUE(accept_latch.Await(kTaskWaitTimeout).result());

  // Advertiser stopAdvertising
  mediums_2.GetBle().GetMedium().StopAdvertising(service_id);
  // Notify device lost
  EXPECT_TRUE(lost_latch.Await(kTaskWaitTimeout).result());
  scan_session.reset();
  DestroyOnExecutor(std::move(scanner), &executor);
}

TEST_F(FastPairScannerImplTest, StopScanning) {
  // Create Fast Pair Scanner and add its observer
  Mediums mediums_1;
  SingleThreadExecutor executor;
  auto scanner = std::make_unique<FastPairScannerImpl>(mediums_1, &executor);
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);
  FastPairScannerObserver observer(scanner.get(), &accept_latch, &lost_latch);
  // Create Advertiser and startAdvertising
  Mediums mediums_2;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  auto scan_session = scanner->StartScanning();
  scan_session.reset();

  mediums_2.GetBle().GetMedium().StopAdvertising(service_id);
  // Device lost event should not be delivered when scan session has terminated.
  EXPECT_FALSE(lost_latch.Await(kShortTimeout).result());
  DestroyOnExecutor(std::move(scanner), &executor);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
