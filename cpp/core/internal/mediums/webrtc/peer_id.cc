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

#include "core/internal/mediums/webrtc/peer_id.h"

#include <sstream>

#include "core/internal/mediums/utils.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {
constexpr int kPeerIdLength = 64;

std::string BytesToStringUppercase(ConstPtr<ByteArray> bytes) {
  std::string hex_string(
      absl::BytesToHexString(std::string(bytes->getData(), bytes->size())));
  absl::AsciiStrToUpper(&hex_string);
  return hex_string;
}
}  // namespace

ConstPtr<PeerId> PeerId::FromRandom(Ptr<HashUtils> hash_utils) {
  return FromSeed(Utils::generateRandomBytes(kPeerIdLength), hash_utils);
}

ConstPtr<PeerId> PeerId::FromSeed(ConstPtr<ByteArray> seed,
                                  Ptr<HashUtils> hash_utils) {
  ScopedPtr<ConstPtr<ByteArray>> full_hash(
      Utils::sha256Hash(hash_utils, seed, kPeerIdLength));
  ScopedPtr<ConstPtr<ByteArray>> hashedSeed(
      MakeConstPtr(new ByteArray(full_hash->getData(), kPeerIdLength / 2)));
  return MakeConstPtr(new PeerId(BytesToStringUppercase(hashedSeed.get())));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
