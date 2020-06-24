#include "core_v2/internal/mediums/webrtc/peer_id.h"

#include <sstream>

#include "core_v2/internal/mediums/utils.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {
constexpr int kPeerIdLength = 64;

std::string BytesToStringUppercase(const ByteArray& bytes) {
  std::string hex_string(
      absl::BytesToHexString(std::string(bytes.data(), bytes.size())));
  absl::AsciiStrToUpper(&hex_string);
  return hex_string;
}
}  // namespace

PeerId PeerId::FromRandom() {
  return FromSeed(Utils::GenerateRandomBytes(kPeerIdLength));
}

PeerId PeerId::FromSeed(const ByteArray& seed) {
  ByteArray full_hash(Utils::Sha256Hash(seed, kPeerIdLength));
  ByteArray hashed_seed(full_hash.data(), kPeerIdLength / 2);
  return PeerId(BytesToStringUppercase(hashed_seed));
}

bool PeerId::IsValid() const { return !id_.empty(); }

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
