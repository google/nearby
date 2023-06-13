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

#include "fastpair/handshake/fast_pair_handshake_lookup.h"

#include <memory>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/common/protocol.h"
#include "fastpair/internal/mediums/mediums.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr absl::string_view kValidModelId("718c17");
constexpr absl::string_view kPubliceAddress("public_address");

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class FastPairHandshakeLookupTest : public ::testing::Test {
 public:
  FastPairHandshakeLookupTest() {
    provider_address_ = adapter_.GetMacAddress();
    device_ = new FastPairDevice(kValidModelId, provider_address_,
                                 Protocol::kFastPairInitialPairing);
    device_->SetPublicAddress(kPubliceAddress);
  }

  ~FastPairHandshakeLookupTest() override { delete device_; }

  void TearDown() override { executor_.Shutdown(); }

  void CreateFastPairHandshkeInstanceForDevice(FastPairDevice& device) {
    CountDownLatch latch(1);
    Mediums mediums;
    EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Create(
        device, mediums,
        [&](FastPairDevice& cb_device, std::optional<PairFailure> failure) {
          EXPECT_EQ(&device, &cb_device);
          EXPECT_TRUE(failure.has_value());
          latch.CountDown();
        },
        &executor_));
    latch.Await();
  }

 protected:
  MediumEnvironmentStarter env_;
  SingleThreadExecutor executor_;
  BluetoothAdapter adapter_;
  BleV2Medium ble_{adapter_};
  std::string provider_address_;

  FastPairDevice* device_ = nullptr;
};

TEST_F(FastPairHandshakeLookupTest, CreateFastPairHandshkeInstanceForDevice) {
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(provider_address_));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(kPubliceAddress));

  CreateFastPairHandshkeInstanceForDevice(*device_);

  // GetFastPairHandshakeWithDevicePtr
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  // GetFastPairHandshakeWithBLEAddress
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(provider_address_));
  // GetFastPairHandshakeWithPublicAddress
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(kPubliceAddress));
  // GetFastPairHandshakeWithDevicePtr
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  // GetFastPairHandshakeWithWrongAddress
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(""));
}

TEST_F(FastPairHandshakeLookupTest, EraseFastPairHandshakeWithDevicePtr) {
  CreateFastPairHandshkeInstanceForDevice(*device_);

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Erase(device_));
  // Already Erased
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  EXPECT_FALSE(
      FastPairHandshakeLookup::GetInstance()->Erase(provider_address_));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(kPubliceAddress));
}

TEST_F(FastPairHandshakeLookupTest, EraseFastPairHandshakeWithBLEAddress) {
  CreateFastPairHandshkeInstanceForDevice(*device_);

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  // Erase Wrong Address
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(""));

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Erase(provider_address_));
  // Already Erased
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(device_));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(kPubliceAddress));
}

TEST_F(FastPairHandshakeLookupTest, EraseFastPairHandshakeWithPublicAddress) {
  CreateFastPairHandshkeInstanceForDevice(*device_);

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Erase(kPubliceAddress));
  // Already Erased
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(device_));
  EXPECT_FALSE(
      FastPairHandshakeLookup::GetInstance()->Erase(provider_address_));
}

TEST_F(FastPairHandshakeLookupTest, ClearAllFastPairHandshakeInstances) {
  CreateFastPairHandshkeInstanceForDevice(*device_);

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  FastPairHandshakeLookup::GetInstance()->Clear();

  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
