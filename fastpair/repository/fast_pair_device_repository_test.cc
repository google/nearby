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

#include "fastpair/repository/fast_pair_device_repository.h"

#include <memory>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::string_view kModelId = "123456";
constexpr absl::string_view kBleAddress = "AA:BB:CC:DD:EE:FF";
constexpr absl::string_view kBtAddress = "12:34:56:78:90:AB";
constexpr absl::string_view kAccountKey = "04b85786180add47fb81a04a8ce6b0de";

TEST(FastPairDeviceRepositoryTest, AddDevice) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository repo(&executor);

  FastPairDevice* device = repo.AddDevice(std::make_unique<FastPairDevice>(
      kModelId, kBleAddress, Protocol::kFastPairInitialPairing));

  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->GetModelId(), kModelId);
  executor.Shutdown();
}

TEST(FastPairDeviceRepositoryTest, FindDeviceByBleAddress) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository repo(&executor);
  repo.AddDevice(std::make_unique<FastPairDevice>(
      kModelId, kBleAddress, Protocol::kFastPairInitialPairing));

  auto opt_device = repo.FindDevice(kBleAddress);

  ASSERT_TRUE(opt_device.has_value());
  FastPairDevice* device = opt_device.value();
  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->GetModelId(), kModelId);
  executor.Shutdown();
}

TEST(FastPairDeviceRepositoryTest, FindDeviceByBtAddress) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository repo(&executor);
  auto fast_pair_device =
      std::make_unique<FastPairDevice>(Protocol::kFastPairInitialPairing);
  fast_pair_device->SetPublicAddress(kBtAddress);
  repo.AddDevice(std::move(fast_pair_device));

  auto opt_device = repo.FindDevice(kBtAddress);

  ASSERT_TRUE(opt_device.has_value());
  FastPairDevice* device = opt_device.value();
  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->GetPublicAddress(), kBtAddress);
  executor.Shutdown();
}

TEST(FastPairDeviceRepositoryTest, FindDeviceByAccountKey) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository repo(&executor);
  auto fast_pair_device =
      std::make_unique<FastPairDevice>(Protocol::kFastPairInitialPairing);
  fast_pair_device->SetPublicAddress(kBtAddress);
  fast_pair_device->SetAccountKey(AccountKey(kAccountKey));
  repo.AddDevice(std::move(fast_pair_device));

  auto opt_device = repo.FindDevice(AccountKey(kAccountKey));

  ASSERT_TRUE(opt_device.has_value());
  FastPairDevice* device = opt_device.value();
  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->GetAccountKey().GetAsBytes(), kAccountKey);
  executor.Shutdown();
}

TEST(FastPairDeviceRepositoryTest, RemoveDevice) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository repo(&executor);
  FastPairDevice* device = repo.AddDevice(std::make_unique<FastPairDevice>(
      kModelId, kBleAddress, Protocol::kFastPairInitialPairing));

  repo.RemoveDevice(device);

  EXPECT_FALSE(repo.FindDevice(kBleAddress).has_value());
  executor.Shutdown();
}

TEST(FastPairDeviceRepositoryTest, RemovingNonRegisteredDeviceIsSafe) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository repo(&executor);
  FastPairDevice* device = repo.AddDevice(std::make_unique<FastPairDevice>(
      kModelId, kBleAddress, Protocol::kFastPairInitialPairing));
  FastPairDevice other_device(Protocol::kFastPairInitialPairing);
  repo.RemoveDevice(device);

  // `device` already removed.
  repo.RemoveDevice(device);
  // `other_device` was never added.
  repo.RemoveDevice(&other_device);

  EXPECT_FALSE(repo.FindDevice(kBleAddress).has_value());
  executor.Shutdown();
}

TEST(FastPairDeviceRepositoryTest, RemovingDeviceCallsCallback) {
  SingleThreadExecutor executor;
  FastPairDeviceRepository repo(&executor);
  FastPairDevice* device = repo.AddDevice(std::make_unique<FastPairDevice>(
      kModelId, kBleAddress, Protocol::kFastPairInitialPairing));
  FastPairDeviceRepository::RemoveDeviceCallback callback =
      [&](const FastPairDevice& device) {};
  repo.AddObserver(&callback);
  repo.RemoveDevice(device);

  EXPECT_FALSE(repo.FindDevice(kBleAddress).has_value());
  executor.Shutdown();
}

}  // namespace

}  // namespace fastpair
}  // namespace nearby
