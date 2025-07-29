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

#include "connections/implementation/mediums/webrtc_peer_id.h"

#include <string>

#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {

constexpr char kTestPeerId[] = "this_is_a_test";

TEST(WebrtcPeerIdTest, GenerateRandomPeerId) {
  WebrtcPeerId peer_id = WebrtcPeerId::FromRandom();
  EXPECT_EQ(64, peer_id.GetId().size());
}

TEST(WebrtcPeerIdTest, GenerateFromSeed) {
  // Values calculated by running actual SHA-256 hash on |seed|.
  std::string seed = "seed";
  std::string expected_peer_id =
      "19B25856E1C150CA834CFFC8B59B23ADBD0EC0389E58EB22B3B64768098D002B";

  ByteArray seed_bytes(seed);
  WebrtcPeerId peer_id = WebrtcPeerId::FromSeed(seed_bytes);

  EXPECT_EQ(64, peer_id.GetId().size());
  EXPECT_EQ(expected_peer_id, peer_id.GetId());
}

TEST(WebrtcPeerIdTest, GetId) {
  WebrtcPeerId peer_id(kTestPeerId);
  EXPECT_EQ(kTestPeerId, peer_id.GetId());
}

TEST(WebrtcPeerIdTest, IsValid) {
  WebrtcPeerId peer_id(kTestPeerId);
  EXPECT_TRUE(peer_id.IsValid());

  WebrtcPeerId peer_id_empty;
  EXPECT_FALSE(peer_id_empty.IsValid());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
