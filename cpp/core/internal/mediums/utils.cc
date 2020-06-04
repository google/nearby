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

#include "core/internal/mediums/utils.h"

#include <cstdint>
#include <sstream>

#include "platform/exception.h"
#include "platform/prng.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {

void Utils::closeSocket(Ptr<BluetoothServerSocket> socket,
                        const std::string& type, const std::string& name) {
  if (!socket.isNull()) {
    Exception::Value e = socket->close();
    if (Exception::NONE != e) {
      if (Exception::IO == e) {
        // TODO(reznor): log.atWarning().withCause(e).log("Failed to close
        // %sSocket %s", type, name);
      }
      return;
    }
    // TODO(reznor): log.atVerbose().log("Closed %sSocket %s", type, name);
  }
}

ConstPtr<ByteArray> Utils::sha256Hash(Ptr<HashUtils> hash_utils,
                                      ConstPtr<ByteArray> source,
                                      size_t length) {
  if (source.isNull()) {
    return ConstPtr<ByteArray>();
  }

  ScopedPtr<ConstPtr<ByteArray>> full_hash(
      hash_utils->sha256(std::string(source->getData(), source->size())));
  return MakeConstPtr(new ByteArray(full_hash->getData(), length));
}

ConstPtr<ByteArray> Utils::legacySha256HashOnlyForPrinting(
    Ptr<HashUtils> hash_utils, ConstPtr<ByteArray> source, size_t length) {
  if (source.isNull()) {
    return ConstPtr<ByteArray>();
  }

  std::string formatted_hex_string = Utils::bytesToPrintableHexString(source);
  ScopedPtr<ConstPtr<ByteArray>> formatted_hex_byte_array(MakeConstPtr(
      new ByteArray(formatted_hex_string.data(), formatted_hex_string.size())));
  return Utils::sha256Hash(hash_utils, formatted_hex_byte_array.get(), length);
}

ConstPtr<ByteArray> Utils::generateRandomBytes(size_t length) {
  Prng rng;
  std::string data;
  data.reserve(length);

  // Adds 4 random bytes per iteration.
  while (length > 0) {
    std::uint32_t val = rng.nextUInt32();
    for (int i = 0; i < 4; i++) {
      data += val & 0xFF;
      val >>= 8;
      length--;

      if (!length) break;
    }
  }

  return MakeConstPtr(new ByteArray(data));
}

std::string Utils::bytesToPrintableHexString(ConstPtr<ByteArray> bytes) {
  std::string hex_string(
      absl::BytesToHexString(std::string(bytes->getData(), bytes->size())));

  // Print out the byte array as a space separated listing of hex bytes.
  std::ostringstream formatted_hex_string_stream;
  formatted_hex_string_stream << "[ ";
  for (int i = 0; i < hex_string.size(); i += 2) {
    formatted_hex_string_stream << "0x";
    // This is safe because we have the guarantee that hex_string is of even
    // length (because a hex encoding will always be double the size of its
    // input).
    formatted_hex_string_stream << hex_string[i] << hex_string[i + 1];
    formatted_hex_string_stream << " ";
  }
  formatted_hex_string_stream << "]";

  return formatted_hex_string_stream.str();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
