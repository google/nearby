// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_ACCOUNT_KEY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_ACCOUNT_KEY_H_

#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/constant.h"
#include "internal/crypto_cros/random.h"

namespace nearby {
namespace fastpair {

class AccountKey {
 public:
  AccountKey() = default;
  explicit AccountKey(absl::string_view bytes) : bytes_(bytes) {}
  explicit AccountKey(const std::vector<uint8_t> bytes) {
    bytes_ = std::string(bytes.begin(), bytes.end());
  }

  static AccountKey CreateRandomKey() {
    std::string key(kAccountKeySize, 0);
    ::crypto::RandBytes(key.data(), kAccountKeySize);
    return AccountKey(key);
  }

  absl::string_view GetAsBytes() const { return bytes_; }

  bool Ok() const { return bytes_.size() == kAccountKeySize; }

  explicit operator bool() const { return Ok(); }

 private:
  std::string bytes_;
};

inline std::ostream& operator<<(std::ostream& stream, const AccountKey& key) {
  stream << "AccountKey(" << absl::BytesToHexString(key.GetAsBytes()) << ")";
  return stream;
}

inline bool operator==(const AccountKey& a, const AccountKey& b) {
  return a.GetAsBytes() == b.GetAsBytes();
}

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_ACCOUNT_KEY_H_
