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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/common/protocol.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr absl::string_view kValidModelId("718c17");
constexpr absl::string_view kBLEAddress("ble_address");
constexpr absl::string_view kPubliceAddress("public_address");
class FastPairHandshakeLookupTest : public ::testing::Test {
 public:
  FastPairHandshakeLookupTest() {
    device_ = new FastPairDevice(kValidModelId, kBLEAddress,
                                 Protocol::kFastPairInitialPairing);
    device_->set_public_address(kPubliceAddress);
  }

  void CreateFastPairHandshkeInstanceForDevice(FastPairDevice& device) {
    CountDownLatch latch(1);
    EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Create(
        device,
        [&](FastPairDevice& cb_device, std::optional<PairFailure> failure) {
          EXPECT_EQ(&device, &cb_device);
          EXPECT_EQ(failure, PairFailure::kCreateGattConnection);
          latch.CountDown();
        }));
    latch.Await();
  }

  FastPairDevice* device_ = nullptr;
};

TEST_F(FastPairHandshakeLookupTest, CreateFastPairHandshkeInstanceForDevice) {
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(kBLEAddress));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(kPubliceAddress));

  CreateFastPairHandshkeInstanceForDevice(*device_);

  // GetFastPairHandshakeWithDevicePtr
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  // GetFastPairHandshakeWithBLEAddress
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(kBLEAddress));
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
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(kBLEAddress));
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(kPubliceAddress));
}

TEST_F(FastPairHandshakeLookupTest, EraseFastPairHandshakeWithBLEAddress) {
  CreateFastPairHandshkeInstanceForDevice(*device_);

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  // Erase Wrong Address
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(""));

  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Erase(kBLEAddress));
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
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Erase(kBLEAddress));
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
