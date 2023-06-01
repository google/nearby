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

#include "fastpair/crypto/fast_pair_message_type.h"

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(FastPairMessageTypeTest, FastPairMessageTypeValue) {
  EXPECT_EQ(static_cast<int>(FastPairMessageType::kUnknown), 0);
  EXPECT_EQ(static_cast<int>(FastPairMessageType::kKeyBasedPairingRequest), 1);
  EXPECT_EQ(static_cast<int>(FastPairMessageType::kKeyBasedPairingResponse), 2);
  EXPECT_EQ(static_cast<int>(FastPairMessageType::kSeekersPasskey), 3);
  EXPECT_EQ(static_cast<int>(FastPairMessageType::kProvidersPasskey), 4);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
