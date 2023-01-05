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

#include "internal/platform/implementation/crypto.h"

#import "absl/strings/string_view.h"
#import "internal/platform/implementation/apple/GNCUtils.h"
#import "internal/platform/implementation/apple/utils.h"

namespace nearby {

void Crypto::Init() {}

ByteArray Crypto::Md5(absl::string_view input) {
  if (input.empty()) return ByteArray();

  return ByteArrayFromNSData(GNCMd5String(ObjCStringFromCppString(input)));
}

ByteArray Crypto::Sha256(absl::string_view input) {
  if (input.empty()) return ByteArray();

  return ByteArrayFromNSData(GNCSha256String(ObjCStringFromCppString(input)));
}

}  // namespace nearby
