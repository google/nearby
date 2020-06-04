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

#include "core_v2/internal/mediums/uuid.h"

#include <iomanip>
#include <sstream>

#include "platform_v2/public/crypto.h"

namespace location {
namespace nearby {
namespace connections {
namespace {
std::ostream& write_hex(std::ostream& os, absl::string_view data) {
  for (const auto b : data) {
    os  << std::setfill('0')
        << std::setw(2)
        << std::hex
        << (static_cast<unsigned int>(b) & 0x0ff);
  }
  return os;
}
}  // namespace

Uuid::Uuid(absl::string_view data) : data_(Crypto::Md5(data)) {
  // Based on the Java counterpart at
  // http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#162.
  data_[6] &= 0x0f;  // Clear version.
  data_[6] |= 0x30;  // Set to version 3.
  data_[8] &= 0x3f;  // Clear variant.
  data_[8] |= 0x80;  // Set to IETF variant.
}

Uuid::Uuid(std::uint64_t most_sig_bits, std::uint64_t least_sig_bits) {
  // Base on the Java counterpart at
  // http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#104.
  data_.reserve(sizeof(most_sig_bits) + sizeof(least_sig_bits));

  data_[0] = static_cast<char>((most_sig_bits >> 56) & 0x0ff);
  data_[1] = static_cast<char>((most_sig_bits >> 48) & 0x0ff);
  data_[2] = static_cast<char>((most_sig_bits >> 40) & 0x0ff);
  data_[3] = static_cast<char>((most_sig_bits >> 32) & 0x0ff);
  data_[4] = static_cast<char>((most_sig_bits >> 24) & 0x0ff);
  data_[5] = static_cast<char>((most_sig_bits >> 16) & 0x0ff);
  data_[6] = static_cast<char>((most_sig_bits >> 8) & 0x0ff);
  data_[7] = static_cast<char>((most_sig_bits >> 0) & 0x0ff);

  data_[8] = static_cast<char>((least_sig_bits >> 56) & 0x0ff);
  data_[9] = static_cast<char>((least_sig_bits >> 48) & 0x0ff);
  data_[10] = static_cast<char>((least_sig_bits >> 40) & 0x0ff);
  data_[11] = static_cast<char>((least_sig_bits >> 32) & 0x0ff);
  data_[12] = static_cast<char>((least_sig_bits >> 24) & 0x0ff);
  data_[13] = static_cast<char>((least_sig_bits >> 16) & 0x0ff);
  data_[14] = static_cast<char>((least_sig_bits >> 8) & 0x0ff);
  data_[15] = static_cast<char>((least_sig_bits >> 0) & 0x0ff);
}

Uuid::operator std::string() const {
  // Based on the Java counterpart at
  // http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#375.
  std::ostringstream md5_hex;
  write_hex(md5_hex, absl::string_view(&data_[0], 4));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&data_[4], 2));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&data_[6], 2));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&data_[8], 2));
  md5_hex << "-";
  write_hex(md5_hex, absl::string_view(&data_[10], 6));

  return md5_hex.str();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
