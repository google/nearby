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

#ifndef PLATFORM_IMPL_WINDOWS_TEST_UTILS_H_
#define PLATFORM_IMPL_WINDOWS_TEST_UTILS_H_

#include <string>

#include "internal/platform/payload_id.h"

#define TEST_BUFFER_SIZE 256
#define TEST_PAYLOAD_ID 64l
#define TEST_STRING                                                            \
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Maecenas "         \
  "eleifend nisl at magna maximus, id finibus mauris ultrices. Mauris "        \
  "interdum efficitur turpis eget auctor. Nullam commodo metus et ante "       \
  "bibendum molestie. Donec iaculis ante nec diam rutrum egestas. Proin "      \
  "maximus metus luctus rutrum congue. Integer et eros nunc. Etiam purus "     \
  "neque, tincidunt eu elementum in, pharetra sit amet magna. Quisque "        \
  "consequat aliquam aliquam. Vestibulum ante ipsum primis in faucibus orci "  \
  "luctus et ultrices posuere cubilia curae; Maecenas a semper eros, a "       \
  "auctor mi. In luctus diam sem, eu pretium nisi porttitor ac. Sed cursus, "  \
  "arcu in bibendum feugiat, leo erat finibus massa, ut tincidunt magna nunc " \
  "eu tellus. Cras feugiat ornare vestibulum. Nullam at ipsum vestibulum "     \
  "sapien luctus dictum ac vel ligula."

namespace test_utils {
std::wstring StringToWideString(const std::string& s);
std::string GetPayloadPath(nearby::PayloadId payload_id);
}  // namespace test_utils

#endif  // PLATFORM_IMPL_WINDOWS_TEST_UTILS_H_
