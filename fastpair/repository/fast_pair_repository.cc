// Copyright 2023 Google LLC
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

#include "fastpair/repository/fast_pair_repository.h"

#include <array>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/base/bluetooth_address.h"
#include "internal/crypto_cros/sha2.h"

namespace nearby {
namespace fastpair {
namespace {
FastPairRepository* g_instance = nullptr;
constexpr int kBluetoothAddressSize = 6;
}

FastPairRepository* FastPairRepository::Get() {
  CHECK(g_instance);
  return g_instance;
}

void FastPairRepository::SetInstance(FastPairRepository* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

FastPairRepository::FastPairRepository() { SetInstance(this); }

FastPairRepository::~FastPairRepository() { SetInstance(nullptr); }

// static
std::string FastPairRepository::GenerateSha256OfAccountKeyAndMacAddress(
    const AccountKey& account_key, absl::string_view public_address) {
  std::string account_key_str = std::string(account_key.GetAsBytes());
  std::vector<uint8_t> concat_bytes(account_key_str.begin(),
                                    account_key_str.end());

  std::vector<uint8_t> public_address_bytes(kBluetoothAddressSize);
  device::ParseBluetoothAddress(
      public_address,
      absl::MakeSpan(public_address_bytes.data(), kBluetoothAddressSize));

  concat_bytes.insert(concat_bytes.end(), public_address_bytes.begin(),
                      public_address_bytes.end());
  std::array<uint8_t, crypto::kSHA256Length> hashed =
      crypto::SHA256Hash(concat_bytes);

  return std::string(hashed.begin(), hashed.end());
}

}  // namespace fastpair
}  // namespace nearby
