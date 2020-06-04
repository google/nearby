// Copyright 2020 Google LLC
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

#include "core/internal/mediums/webrtc/peer_id.h"

#include "platform/api/hash_utils.h"
#include "platform/byte_array.h"
#include "platform/ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

class MockHashUtils : public HashUtils {
 public:
  MOCK_METHOD(ConstPtr<ByteArray>, md5, (const std::string& input), (override));
  MOCK_METHOD(ConstPtr<ByteArray>, sha256, (const std::string& input),
              (override));
};

}  // namespace

TEST(PeerIdTest, GenerateRandomPeerId) {
  // These are actual SHA-256 values for |seed| = "seed".
  std::string hashed_output =
      "19b25856e1c150ca834cffc8b59b23adbd0ec0389e58eb22b3b64768098d002b";
  std::string expected_peer_id =
      "19B25856E1C150CA834CFFC8B59B23ADBD0EC0389E58EB22B3B64768098D002B";

  Ptr<testing::NiceMock<MockHashUtils>> mock_hash_utils(
      MakePtr(new MockHashUtils()));
  ON_CALL(*mock_hash_utils.get(), sha256(testing::_))
      .WillByDefault(testing::Return(
          MakeConstPtr(new ByteArray(absl::HexStringToBytes(hashed_output)))));
  EXPECT_CALL(*mock_hash_utils.get(), sha256(testing::_));

  ConstPtr<PeerId> peer_id = PeerId::FromRandom(mock_hash_utils);
  ASSERT_EQ(64, peer_id->GetId().size());
  ASSERT_EQ(expected_peer_id, peer_id->GetId());
}

TEST(PeerIdTest, GenerateFromSeed) {
  // Values calculated by running actual SHA-256 hash on |seed|.
  std::string seed = "sesdfed";
  std::string hashed_output =
      "19b25856e1c150ca834cffc8b59b23adbd0ec0389e58eb22b3b64768098d002b";
  std::string expected_peer_id =
      "19B25856E1C150CA834CFFC8B59B23ADBD0EC0389E58EB22B3B64768098D002B";

  Ptr<testing::NiceMock<MockHashUtils>> mock_hash_utils(
      MakePtr(new MockHashUtils()));
  ON_CALL(*mock_hash_utils.get(), sha256(testing::Eq(seed)))
      .WillByDefault(testing::Return(
          MakeConstPtr(new ByteArray(absl::HexStringToBytes(hashed_output)))));
  EXPECT_CALL(*mock_hash_utils.get(), sha256(testing::Eq(seed)));

  ConstPtr<PeerId> peer_id =
      PeerId::FromSeed(MakeConstPtr(new ByteArray(seed)), mock_hash_utils);

  ASSERT_EQ(64, peer_id->GetId().size());
  ASSERT_EQ(expected_peer_id, peer_id->GetId());
}

TEST(PeerIdTest, GetId) {
  const std::string id = "this_is_a_test";
  PeerId peer_id(id);
  ASSERT_EQ(id, peer_id.GetId());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
