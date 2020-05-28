#include "core_v2/internal/mediums/utils.h"

#include <memory>
#include <string>

#include "platform_v2/base/prng.h"
#include "platform_v2/public/crypto.h"

namespace location {
namespace nearby {
namespace connections {

ByteArray Utils::GenerateRandomBytes(size_t length) {
  Prng rng;
  std::string data;
  data.reserve(length);

  // Adds 4 random bytes per iteration.
  while (length > 0) {
    std::uint32_t val = rng.NextUint32();
    for (int i = 0; i < 4; i++) {
      data += val & 0xFF;
      val >>= 8;
      length--;

      if (!length) break;
    }
  }

  return ByteArray(data);
}

ByteArray Utils::Sha256Hash(const ByteArray& source, size_t length) {
  ByteArray full_hash(length);
  full_hash.CopyAt(0, Crypto::Sha256(std::string(source)));
  return full_hash;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
