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

#include "fastpair/dart/fast_pair_wrapper_impl.h"

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace FastPairWrapperUnitTests {
class FastPairWrapperImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    wrapper_.reset();
    wrapper_ = std::make_unique<FastPairWrapperImpl>();
  }

 protected:
  std::shared_ptr<FastPairWrapperImpl> wrapper_;
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(FastPairWrapperImplTest, StartScanningSuccess) {
  env_.Start();
  EXPECT_FALSE(wrapper_->IsScanning());
  EXPECT_FALSE(wrapper_->IsPairing());
  EXPECT_FALSE(wrapper_->IsServerAccessing());
  wrapper_->StartScan();
  SystemClock::Sleep(absl::Milliseconds(200));
  EXPECT_TRUE(wrapper_->IsScanning());
  env_.Stop();
}

}  // namespace FastPairWrapperUnitTests
}  // namespace fastpair
}  // namespace nearby
