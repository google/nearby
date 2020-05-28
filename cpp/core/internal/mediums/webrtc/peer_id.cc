#include "core/internal/mediums/webrtc/peer_id.h"

#include <sstream>

#include "core/internal/mediums/utils.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {
constexpr int kPeerIdLength = 64;

std::string BytesToStringUppercase(ConstPtr<ByteArray> bytes) {
  std::string hex_string(
      absl::BytesToHexString(std::string(bytes->getData(), bytes->size())));
  absl::AsciiStrToUpper(&hex_string);
  return hex_string;
}
}  // namespace

ConstPtr<PeerId> PeerId::FromRandom(Ptr<HashUtils> hash_utils) {
  return FromSeed(Utils::generateRandomBytes(kPeerIdLength), hash_utils);
}

ConstPtr<PeerId> PeerId::FromSeed(ConstPtr<ByteArray> seed,
                                  Ptr<HashUtils> hash_utils) {
  ScopedPtr<ConstPtr<ByteArray>> full_hash(
      Utils::sha256Hash(hash_utils, seed, kPeerIdLength));
  ScopedPtr<ConstPtr<ByteArray>> hashedSeed(
      MakeConstPtr(new ByteArray(full_hash->getData(), kPeerIdLength / 2)));
  return MakeConstPtr(new PeerId(BytesToStringUppercase(hashedSeed.get())));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
