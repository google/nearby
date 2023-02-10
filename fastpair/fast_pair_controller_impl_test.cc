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
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace FastPairControllerUnitTests {
class FastPairControllerImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    controller_.reset();
    controller_ = std::make_unique<FastPairControllerImpl>();
  }

 protected:
  std::shared_ptr<FastPairControllerImpl> controller_;
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(FastPairControllerImplTest, StartScanningSuccess) {
  env_.Start();
  EXPECT_FALSE(controller_->IsScanning());
  EXPECT_FALSE(controller_->IsPairing());
  EXPECT_FALSE(controller_->IsServerAccessing());
  controller_->StartScan();
  SystemClock::Sleep(absl::Milliseconds(200));
  EXPECT_TRUE(controller_->IsScanning());
  env_.Stop();
}

}  // namespace FastPairControllerUnitTests
}  // namespace fastpair
}  // namespace nearby
