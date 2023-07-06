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

#include "fastpair/repository/fast_pair_repository.h"

#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/account_key.h"

namespace nearby {
namespace fastpair {
namespace {
TEST(FastPairRepositoryTest, TestSHA256HashFunction) {
  absl::string_view kTestClassicAddress("20:64:DE:40:F8:93");
  absl::string_view kExpectedSha256Hash(
      "6353c0075a35b7d81bb30a6190ab246da4b8c55a6111d387400579133c090ed8");
  absl::string_view kAccountKey("04b85786180add47fb81a04a8ce6b0de");

  EXPECT_EQ(absl::BytesToHexString(
                FastPairRepository::GenerateSha256OfAccountKeyAndMacAddress(
                    AccountKey(absl::HexStringToBytes(kAccountKey)),
                    kTestClassicAddress)),
            kExpectedSha256Hash);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
