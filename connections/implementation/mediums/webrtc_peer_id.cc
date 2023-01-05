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

#include "connections/implementation/mediums/webrtc_peer_id.h"

#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "connections/implementation/mediums/utils.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {
constexpr int kPeerIdLength = 64;

std::string BytesToStringUppercase(const ByteArray& bytes) {
  std::string hex_string(
      absl::BytesToHexString(std::string(bytes.data(), bytes.size())));
  absl::AsciiStrToUpper(&hex_string);
  return hex_string;
}
}  // namespace

WebrtcPeerId WebrtcPeerId::FromRandom() {
  return FromSeed(Utils::GenerateRandomBytes(kPeerIdLength));
}

WebrtcPeerId WebrtcPeerId::FromSeed(const ByteArray& seed) {
  ByteArray full_hash(Utils::Sha256Hash(seed, kPeerIdLength));
  ByteArray hashed_seed(full_hash.data(), kPeerIdLength / 2);
  return WebrtcPeerId(BytesToStringUppercase(hashed_seed));
}

bool WebrtcPeerId::IsValid() const { return !id_.empty(); }

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
