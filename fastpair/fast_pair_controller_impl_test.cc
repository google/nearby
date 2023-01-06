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

#include "fastpair/fast_pair_controller_impl.h"

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace FastPairControllerUnitTests {
class FastPairControllerImplTest : public ::testing::Test {
 public:
  FastPairControllerImplTest() = default;
  ~FastPairControllerImplTest() override = default;
  void SetUp() override { controller_ = CreateController(); }

 protected:
  std::unique_ptr<FastPairControllerImpl> CreateController() {
    auto controller = std::make_unique<FastPairControllerImpl>();
    return controller;
  }
  std::unique_ptr<FastPairControllerImpl> controller_;
};

TEST_F(FastPairControllerImplTest, StartScanningSuccess) {
  EXPECT_FALSE(controller_->IsScanning());
  EXPECT_FALSE(controller_->IsPairing());
  EXPECT_FALSE(controller_->IsServerAccessing());
  controller_->StartScan();
  EXPECT_TRUE(controller_->IsScanning());
}

}  // namespace FastPairControllerUnitTests
}  // namespace fastpair
}  // namespace nearby
