#include <random>

#include <systemd/sd-id128.h>

#include "absl/strings/str_cat.h"
#include "internal/platform/implementation/linux/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
std::optional<Uuid> UuidFromString(const std::string &uuid_str) {
  sd_id128_t uuid;
  if (auto ret = sd_id128_from_string(uuid_str.c_str(), &uuid); ret < 0)
    return std::nullopt;

  const int ONE = 1;
  if (*(reinterpret_cast<const char *>(&ONE)) ==
      1) {  // On a little endian platform
    uint64_t msb = static_cast<uint64_t>(uuid.bytes[7]) +
                   (static_cast<uint64_t>(uuid.bytes[6]) << 8) +
                   (static_cast<uint64_t>(uuid.bytes[5]) << 16) +
                   (static_cast<uint64_t>(uuid.bytes[4]) << 24) +
                   (static_cast<uint64_t>(uuid.bytes[3]) << 32) +
                   (static_cast<uint64_t>(uuid.bytes[2]) << 40) +
                   (static_cast<uint64_t>(uuid.bytes[1]) << 48) +
                   (static_cast<uint64_t>(uuid.bytes[0]) << 56);
    uint64_t lsb = static_cast<uint64_t>(uuid.bytes[15]) +
                   (static_cast<uint64_t>(uuid.bytes[14]) << 8) +
                   (static_cast<uint64_t>(uuid.bytes[13]) << 16) +
                   (static_cast<uint64_t>(uuid.bytes[12]) << 24) +
                   (static_cast<uint64_t>(uuid.bytes[11]) << 32) +
                   (static_cast<uint64_t>(uuid.bytes[10]) << 40) +
                   (static_cast<uint64_t>(uuid.bytes[9]) << 48) +
                   (static_cast<uint64_t>(uuid.bytes[8]) << 56);
    return Uuid(msb, lsb);
  }

  return Uuid(uuid.qwords[0], uuid.qwords[1]);
}

std::optional<std::string> NewUuidStr() {
  sd_id128_t id;
  char id_cstr[SD_ID128_UUID_STRING_MAX];
  if (auto ret = sd_id128_randomize(&id); ret < 0) {
    LOG(ERROR) << __func__ << ": could not generate a random UUID: "
                       << std::strerror(ret);
    return std::nullopt;
  }

  return std::string(sd_id128_to_uuid_string(id, id_cstr));
}

std::string RandString(std::string allowed_chars, size_t length) {
  thread_local static std::random_device device{};

  std::mt19937 gen{device()};
  std::uniform_int_distribution<size_t> dist(0, allowed_chars.length() - 1);

  std::string s;
  s.reserve(length);

  for (auto i = 0; i < length; i++) {
    s += allowed_chars[dist(gen)];
  }

  return s;
}

std::string RandSSID() {
  std::string allowed_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789";

  return absl::StrCat("DIRECT-", RandString(allowed_chars, 25));
}

std::string RandWPAPassphrase() {
  std::string allowed_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789"
      "!\"#$%&'()*+,-./[\\]^_`~{|}";

  return RandString(allowed_chars, 63);
}
}  // namespace linux
}  // namespace nearby
