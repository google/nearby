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

#include "fastpair/common/pair_failure.h"

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(PairFailureTest, PairFailureValue) {
  EXPECT_EQ(static_cast<int>(PairFailure::kUnknown), 0);
  EXPECT_EQ(static_cast<int>(PairFailure::kCreateGattConnection), 1);
  EXPECT_EQ(static_cast<int>(PairFailure::kGattServiceDiscovery), 2);
  EXPECT_EQ(static_cast<int>(PairFailure::kGattServiceDiscoveryTimeout), 3);
  EXPECT_EQ(static_cast<int>(PairFailure::kDataEncryptorRetrieval), 4);
  EXPECT_EQ(
      static_cast<int>(PairFailure::kKeyBasedPairingCharacteristicDiscovery),
      5);
  EXPECT_EQ(static_cast<int>(PairFailure::kPasskeyCharacteristicDiscovery), 6);
  EXPECT_EQ(static_cast<int>(PairFailure::kAccountKeyCharacteristicDiscovery),
            7);
  EXPECT_EQ(
      static_cast<int>(PairFailure::kKeyBasedPairingCharacteristicSubscription),
      8);
  EXPECT_EQ(static_cast<int>(PairFailure::kPasskeyCharacteristicSubscription),
            9);
  EXPECT_EQ(static_cast<int>(
                PairFailure::kKeyBasedPairingCharacteristicSubscriptionTimeout),
            10);
  EXPECT_EQ(
      static_cast<int>(PairFailure::kPasskeyCharacteristicSubscriptionTimeout),
      11);
  EXPECT_EQ(static_cast<int>(PairFailure::kKeyBasedPairingCharacteristicWrite),
            12);
  EXPECT_EQ(static_cast<int>(PairFailure::kPasskeyPairingCharacteristicWrite),
            13);
  EXPECT_EQ(static_cast<int>(PairFailure::kAccountKeyCharacteristicWrite), 14);
  EXPECT_EQ(static_cast<int>(PairFailure::kKeyBasedPairingResponseTimeout), 15);
  EXPECT_EQ(static_cast<int>(PairFailure::kPasskeyResponseTimeout), 16);
  EXPECT_EQ(
      static_cast<int>(PairFailure::kKeybasedPairingResponseDecryptFailure),
      17);
  EXPECT_EQ(
      static_cast<int>(PairFailure::kIncorrectKeyBasedPairingResponseType), 18);
  EXPECT_EQ(static_cast<int>(PairFailure::kPasskeyDecryptFailure), 19);
  EXPECT_EQ(static_cast<int>(PairFailure::kIncorrectPasskeyResponseType), 20);
  EXPECT_EQ(static_cast<int>(PairFailure::kPasskeyMismatch), 21);
  EXPECT_EQ(static_cast<int>(
                PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts),
            22);
  EXPECT_EQ(static_cast<int>(PairFailure::kDeviceLostMidPairing), 23);
  EXPECT_EQ(static_cast<int>(PairFailure::kPairingAndConnect), 24);
  EXPECT_EQ(static_cast<int>(PairFailure::kPairingTimeout), 25);
  EXPECT_EQ(static_cast<int>(PairFailure::kWriteAccountKeyToFootprints), 26);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
