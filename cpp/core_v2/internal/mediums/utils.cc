#include "core_v2/internal/mediums/utils.h"

#include <memory>
#include <string>

#include "platform_v2/base/prng.h"
#include "platform_v2/public/crypto.h"

namespace location {
namespace nearby {
namespace connections {

namespace {
constexpr absl::string_view kUpgradeServiceIdPostfix = "_UPGRADE";
}

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
  return Utils::Sha256Hash(std::string(source), length);
}

ByteArray Utils::Sha256Hash(const std::string& source, size_t length) {
  ByteArray full_hash(length);
  full_hash.CopyAt(0, Crypto::Sha256(source));
  return full_hash;
}

std::string Utils::WrapUpgradeServiceId(const std::string& service_id) {
  if (service_id.empty()) {
    return {};
  }
  return service_id + std::string(kUpgradeServiceIdPostfix);
}

std::string Utils::UnwrapUpgradeServiceId(
    const std::string& upgrade_service_id) {
  auto pos = upgrade_service_id.find(std::string(kUpgradeServiceIdPostfix));
  if (pos != std::string::npos) {
    return std::string(upgrade_service_id, 0, pos);
  }
  return upgrade_service_id;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
