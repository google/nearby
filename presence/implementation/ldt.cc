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

#include "presence/implementation/ldt.h"

#include <algorithm>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#if USE_RUST_LDT == 1
#include "third_party/nearby_rust/presence/ldt_np_adv_ffi/include/np_ldt.h"
#else
#include "presence/implementation/np_ldt.h"
#endif /* USE_RUST_LDT */

namespace nearby {
namespace presence {

namespace {
// NP LDT library says that 0 is returned when `NpLdtCreate()` fails.
constexpr NpLdtHandle kInvalidLdtHandle = static_cast<NpLdtHandle>(0);

template <class T>
T FromStringView(absl::string_view data) {
  T result{
      .bytes = {0},
  };
  memcpy(result.bytes, data.data(),
         std::min(sizeof(result.bytes), data.size()));
  return result;
}
}  // namespace

LdtEncryptor::LdtEncryptor(LdtEncryptor&& other)
    : ldt_handle_(other.ldt_handle_) {
  other.ldt_handle_ = kInvalidLdtHandle;
}
LdtEncryptor::~LdtEncryptor() {
  if (ldt_handle_ != kInvalidLdtHandle) {
    NpLdtClose(ldt_handle_);
  }
}

absl::StatusOr<LdtEncryptor> LdtEncryptor::Create(
    absl::string_view key_seed, absl::string_view known_hmac) {
  NpLdtHandle handle =
      NpLdtCreate(FromStringView<NpLdtKeySeed>(key_seed),
                  FromStringView<NpMetadataKeyHmac>(known_hmac));
  if (handle == kInvalidLdtHandle) {
    return absl::UnavailableError("Failed to create LDT encryptor");
  }

  return LdtEncryptor(handle);
}

absl::StatusOr<std::string> LdtEncryptor::Encrypt(absl::string_view data,
                                                  absl::string_view salt) {
  std::string encrypted = std::string(data);
  NP_LDT_RESULT result =
      NpLdtEncrypt(ldt_handle_, reinterpret_cast<uint8_t*>(encrypted.data()),
                   encrypted.size(), FromStringView<NpLdtSalt>(salt));
  if (result == NP_LDT_SUCCESS) {
    return encrypted;
  }
  return absl::InternalError(
      absl::StrFormat("LDT encryption failed, errorcode %d", result));
}

absl::StatusOr<std::string> LdtEncryptor::DecryptAndVerify(
    absl::string_view data, absl::string_view salt) {
  std::string encrypted = std::string(data);
  NP_LDT_RESULT result = NpLdtDecryptAndVerify(
      ldt_handle_, reinterpret_cast<uint8_t*>(encrypted.data()),
      encrypted.size(), FromStringView<NpLdtSalt>(salt));
  if (result == NP_LDT_SUCCESS) {
    return encrypted;
  }
  return absl::InternalError(
      absl::StrFormat("LDT encryption failed, errorcode %d", result));
}

}  // namespace presence
}  // namespace nearby
