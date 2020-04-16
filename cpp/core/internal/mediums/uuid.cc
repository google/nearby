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

#include "core/internal/mediums/uuid.h"

#include <iomanip>
#include <sstream>

#include "platform/api/hash_utils.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
UUID<Platform>::UUID(const string& data) {
  // Based on the Java counterpart at
  // http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#162.
  ScopedPtr<Ptr<HashUtils> > scoped_hash_utils(Platform::createHashUtils());
  ScopedPtr<ConstPtr<ByteArray> > scoped_md5_bytes(
      scoped_hash_utils->md5(data));
  data_.assign(scoped_md5_bytes->getData(), scoped_md5_bytes->size());

  data_[6] &= 0x0f;  // Clear version.
  data_[6] |= 0x30;  // Set to version 3.
  data_[8] &= 0x3f;  // Clear variant.
  data_[8] |= 0x80;  // Set to IETF variant.
}

template <typename Platform>
UUID<Platform>::UUID(std::int64_t most_sig_bits, std::int64_t least_sig_bits) {
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

template <typename Platform>
UUID<Platform>::~UUID() {}

template <typename Platform>
string UUID<Platform>::str() {
  // Based on the Java counterpart at
  // http://androidxref.com/8.0.0_r4/xref/libcore/ojluni/src/main/java/java/util/UUID.java#375.

  // The masking with 0x0ff is essential because we're taking 8-bit bytes and
  // casting them to integers (which, depending on the platform, are 16- or
  // 32-bits wide); without that, we get a leading FF (16-bit) or FFFFFF
  // (32-bit) when the MSB of the 8-bit byte is 1.
  //
  // And the cast to an integer is required because std::hex only takes effect
  // on integral types (and no, uint8_t doesn't activate it).
#define BYTE_TO_HEX(b)                          \
  std::setfill('0') << std::setw(2) << std::hex \
                    << (static_cast<unsigned int>(b) & 0x0ff)

  std::ostringstream md5_hex;

  md5_hex << BYTE_TO_HEX(data_[0]);
  md5_hex << BYTE_TO_HEX(data_[1]);
  md5_hex << BYTE_TO_HEX(data_[2]);
  md5_hex << BYTE_TO_HEX(data_[3]);
  md5_hex << "-";
  md5_hex << BYTE_TO_HEX(data_[4]);
  md5_hex << BYTE_TO_HEX(data_[5]);
  md5_hex << "-";
  md5_hex << BYTE_TO_HEX(data_[6]);
  md5_hex << BYTE_TO_HEX(data_[7]);
  md5_hex << "-";
  md5_hex << BYTE_TO_HEX(data_[8]);
  md5_hex << BYTE_TO_HEX(data_[9]);
  md5_hex << "-";
  md5_hex << BYTE_TO_HEX(data_[10]);
  md5_hex << BYTE_TO_HEX(data_[11]);
  md5_hex << BYTE_TO_HEX(data_[12]);
  md5_hex << BYTE_TO_HEX(data_[13]);
  md5_hex << BYTE_TO_HEX(data_[14]);
  md5_hex << BYTE_TO_HEX(data_[15]);

  return md5_hex.str();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
