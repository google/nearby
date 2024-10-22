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

#include "internal/platform/uuid.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include <openssl/base.h>
#include <openssl/digest.h>

namespace nearby {
namespace {
std::ostream& write_hex(std::ostream& os, absl::string_view data) {
  for (const auto b : data) {
    os << std::setfill('0') << std::setw(2) << std::hex << std::uppercase
       << (static_cast<unsigned int>(b) & 0x0ff);
  }
  return os;
}

ByteArray Hash(absl::string_view input, const EVP_MD* algo) {
  unsigned int md_out_size = EVP_MAX_MD_SIZE;
  uint8_t digest_buffer[EVP_MAX_MD_SIZE];
  if (input.empty()) return {};

  if (!EVP_Digest(input.data(), input.size(), digest_buffer, &md_out_size, algo,
                  nullptr))
    return {};

  return ByteArray{reinterpret_cast<char*>(digest_buffer), md_out_size};
}

ByteArray Md5(absl::string_view input) { return Hash(input, EVP_md5()); }

}  // namespace

// Based on the Java implementation
// http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#191
std::optional<Uuid> Uuid::FromString(absl::string_view data) {
  std::vector<std::string> components = absl::StrSplit(data, '-');
  if (components.size() != 5) {
    return std::nullopt;
  }

  for (std::string& component : components) {
    component = absl::StrCat("0x", component);
  }

  int64_t most_sig_bits = 0L;
  int64_t least_sig_bits = 0L;
  int64_t temp;
  if (!absl::SimpleHexAtoi(components[0], &temp)) {
    return std::nullopt;
  }
  most_sig_bits = temp;
  most_sig_bits <<= 16;
  if (!absl::SimpleHexAtoi(components[1], &temp)) {
    return std::nullopt;
  }
  most_sig_bits |= temp;
  most_sig_bits <<= 16;
  if (!absl::SimpleHexAtoi(components[2], &temp)) {
    return std::nullopt;
  }
  most_sig_bits |= temp;

  if (!absl::SimpleHexAtoi(components[3], &temp)) {
    return std::nullopt;
  }
  least_sig_bits = temp;
  least_sig_bits <<= 48;
  if (!absl::SimpleHexAtoi(components[4], &temp)) {
    return std::nullopt;
  }
  least_sig_bits |= temp;

  return Uuid(most_sig_bits, least_sig_bits);
}

Uuid::Uuid(absl::string_view data) {
  // Based on the Java counterpart at
  // http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#162.
  std::string md5_data(Md5(data));
  md5_data[6] &= 0x0f;  // Clear version.
  md5_data[6] |= 0x30;  // Set to version 3.
  md5_data[8] &= 0x3f;  // Clear variant.
  md5_data[8] |= 0x80;  // Set to IETF variant.

  std::uint64_t msb = 0;
  std::uint64_t lsb = 0;
  for (int i = 0; i < 8; i++) {
    msb = (msb << 8) | (md5_data[i] & 0xff);
  }
  for (int i = 8; i < 16; i++) {
    lsb = (lsb << 8) | (md5_data[i] & 0xff);
  }
  most_sig_bits_ = msb;
  least_sig_bits_ = lsb;
}

Uuid::operator std::string() const { return ToCanonicalString(data()); }

std::string Uuid::Get16BitAsString() const {
  std::ostringstream sixteen_bit_string;

  char data = static_cast<char>((most_sig_bits_ >> 40) & 0x0ff);
  write_hex(sixteen_bit_string, absl::string_view(&data, 1));
  data = static_cast<char>((most_sig_bits_ >> 32) & 0x0ff);
  write_hex(sixteen_bit_string, absl::string_view(&data, 1));

  return sixteen_bit_string.str();
}

std::array<char, 16> Uuid::data() const {
  std::array<char, 16> data;
  data[0] = (most_sig_bits_ >> 56) & 0x0ff;
  data[1] = (most_sig_bits_ >> 48) & 0x0ff;
  data[2] = (most_sig_bits_ >> 40) & 0x0ff;
  data[3] = (most_sig_bits_ >> 32) & 0x0ff;
  data[4] = (most_sig_bits_ >> 24) & 0x0ff;
  data[5] = (most_sig_bits_ >> 16) & 0x0ff;
  data[6] = (most_sig_bits_ >> 8) & 0x0ff;
  data[7] = (most_sig_bits_ >> 0) & 0x0ff;
  data[8] = (least_sig_bits_ >> 56) & 0x0ff;
  data[9] = (least_sig_bits_ >> 48) & 0x0ff;
  data[10] = (least_sig_bits_ >> 40) & 0x0ff;
  data[11] = (least_sig_bits_ >> 32) & 0x0ff;
  data[12] = (least_sig_bits_ >> 24) & 0x0ff;
  data[13] = (least_sig_bits_ >> 16) & 0x0ff;
  data[14] = (least_sig_bits_ >> 8) & 0x0ff;
  data[15] = (least_sig_bits_ >> 0) & 0x0ff;
  return data;
}

std::string Uuid::ToCanonicalString(const std::array<char, 16>& data) const {
  // Based on the Java counterpart at
  // http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#375.
  std::ostringstream md5_hex;
  std::string uuid_string(data.data(), data.size());
  write_hex(md5_hex, absl::string_view(&uuid_string[0], 4));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&uuid_string[4], 2));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&uuid_string[6], 2));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&uuid_string[8], 2));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&uuid_string[10], 6));

  return md5_hex.str();
}

bool Uuid::operator<(const Uuid& rhs) const {
  return GetMostSigBits() == rhs.GetMostSigBits()
             ? GetLeastSigBits() < rhs.GetLeastSigBits()
             : GetMostSigBits() < rhs.GetLeastSigBits();
}

bool Uuid::operator==(const Uuid& rhs) const {
  return GetMostSigBits() == rhs.GetMostSigBits() &&
         GetLeastSigBits() == rhs.GetLeastSigBits();
}

bool Uuid::operator!=(const Uuid& rhs) const { return !(*this == rhs); }

}  // namespace nearby
