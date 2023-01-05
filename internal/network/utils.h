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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_UTILS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_UTILS_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/network/url.h"

namespace nearby {
namespace network {

// Encodes string to URL-encoded format. URLs can only be sent over the Internet
// using the ASCII character-set. This method converts characters into a format
// that can be transmitted over the Internet.
//
// @param str The string to be encoded.
// @return    Encoded URL string from input.
std::string UrlEncode(absl::string_view str);

// Decodes URL string to normal string.
//
// @param url_string URL string to be decoded.
// @return     Decoded string.
std::string UrlDecode(absl::string_view url_string);

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_UTILS_H_
