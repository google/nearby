#include "platform_v2/api/crypto.h"

#include <cstdint>
#include <string>

#include "platform_v2/base/byte_array.h"
#include "absl/strings/string_view.h"
#include "openssl/digest.h"

namespace location {
namespace nearby {

// Initialize global crypto state.
void Crypto::Init() {}

static ByteArray Hash(absl::string_view input, const EVP_MD* algo) {
  unsigned int md_out_size = EVP_MAX_MD_SIZE;
  uint8_t digest_buffer[EVP_MAX_MD_SIZE];
  if (input.empty()) return {};

  if (!EVP_Digest(input.data(), input.size(), digest_buffer, &md_out_size, algo,
                  nullptr))
    return {};

  return ByteArray{reinterpret_cast<char*>(digest_buffer), md_out_size};
}

// Return MD5 hash of input.
ByteArray Crypto::Md5(absl::string_view input) {
  return Hash(input, EVP_md5());
}

// Return SHA256 hash of input.
ByteArray Crypto::Sha256(absl::string_view input) {
  return Hash(input, EVP_sha256());
}

}  // namespace nearby
}  // namespace location
