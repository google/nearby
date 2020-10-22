#include "core/internal/mediums/webrtc/peer_id.h"

#include <memory>

#include "platform/base/byte_array.h"
#include "platform/public/crypto.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

TEST(PeerIdTest, GenerateRandomPeerId) {
  PeerId peer_id = PeerId::FromRandom();
  EXPECT_EQ(64, peer_id.GetId().size());
}

TEST(PeerIdTest, GenerateFromSeed) {
  // Values calculated by running actual SHA-256 hash on |seed|.
  std::string seed = "seed";
  std::string expected_peer_id =
      "19B25856E1C150CA834CFFC8B59B23ADBD0EC0389E58EB22B3B64768098D002B";

  ByteArray seed_bytes(seed);
  PeerId peer_id = PeerId::FromSeed(seed_bytes);

  EXPECT_EQ(64, peer_id.GetId().size());
  EXPECT_EQ(expected_peer_id, peer_id.GetId());
}

TEST(PeerIdTest, GetId) {
  const std::string id = "this_is_a_test";
  PeerId peer_id(id);
  EXPECT_EQ(id, peer_id.GetId());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
