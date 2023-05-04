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

#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace FastPairWrapperUnitTests {

TEST(FastPairWrapperImplTest, StartScanningSuccess) {
  FastPairWrapperImpl wrapper;
  EXPECT_FALSE(wrapper.IsScanning());
  EXPECT_FALSE(wrapper.IsPairing());
  EXPECT_FALSE(wrapper.IsServerAccessing());
}

}  // namespace FastPairWrapperUnitTests
}  // namespace fastpair
}  // namespace nearby
