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

#include "internal/network/utils.h"

#include <cctype>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>

namespace nearby {
namespace network {

// Encodes string to URL-encoded string.
//
// If a character c is a decimal digit, an uppercase or lowercase letter, or in
// [-_.!~*'()] then nothing is done; otherwise c is encoded as %XX where XX is
// the hex value of the character c.
std::string UrlEncode(absl::string_view str) {
  std::ostringstream encoded_string_stream;
  encoded_string_stream.fill(0);
  encoded_string_stream << std::hex;
  auto it = str.begin();
  while (it != str.end()) {
    char ch = (*it);
    if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '!' ||
        ch == '~' || ch == '*' || ch == '\'' || ch == '(' || ch == ')') {
      encoded_string_stream << ch;
    } else {
      encoded_string_stream << std::uppercase;
      encoded_string_stream << '%' << std::setw(2) << (ch & 0xFF);
      encoded_string_stream << std::nouppercase;
    }

    ++it;
  }

  return encoded_string_stream.str();
}

// Decodes URL string to normal string.
//
// Does the reverse operation comparing to UrlEncode.
// More details can refer to https://www.ietf.org/rfc/rfc2396.txt
std::string UrlDecode(absl::string_view url_string) {
  std::ostringstream decoded_string_stream;
  decoded_string_stream.fill(0);
  decoded_string_stream << std::hex;
  auto it = url_string.begin();

  while (it != url_string.end()) {
    char ch = (*it);

    if (ch == '%') {
      if (it + 1 != url_string.end() && it + 2 != url_string.end()) {
        char chh = *(it + 1);
        char chl = *(it + 2);
        ch = (chh >= '0' && chh <= '9' ? chh - '0'
                                       : std::tolower(chh) - 'a' + 10)
                 << 4 |
             (chl >= '0' && chl <= '9' ? chl - '0'
                                       : std::tolower(chl) - 'a' + 10);
        decoded_string_stream << ch;
        it += 3;
        continue;
      }
    } else {
      decoded_string_stream << ch;
    }

    ++it;
  }

  return decoded_string_stream.str();
}

}  // namespace network
}  // namespace nearby
