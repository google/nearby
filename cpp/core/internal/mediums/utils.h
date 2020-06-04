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

#ifndef CORE_INTERNAL_MEDIUMS_UTILS_H_
#define CORE_INTERNAL_MEDIUMS_UTILS_H_

#include "platform/api/bluetooth_classic.h"
#include "platform/api/hash_utils.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

class Utils {
 public:
  static void closeSocket(Ptr<BluetoothServerSocket> socket,
                          const std::string& type, const std::string& name);
  static ConstPtr<ByteArray> sha256Hash(Ptr<HashUtils> hash_utils,
                                        ConstPtr<ByteArray> source,
                                        size_t length);
  static ConstPtr<ByteArray> legacySha256HashOnlyForPrinting(
      Ptr<HashUtils> hash_utils, ConstPtr<ByteArray> source, size_t length);

  static ConstPtr<ByteArray> generateRandomBytes(size_t length);

 private:
  static std::string bytesToPrintableHexString(ConstPtr<ByteArray> bytes);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_UTILS_H_
