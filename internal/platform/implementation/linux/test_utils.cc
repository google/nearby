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

#include <algorithm>
#include <codecvt>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/implementation/linux/test_utils.h"

namespace test_utils {
std::wstring StringToWideString(const std::string& s) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.from_bytes(s);
}

std::string GetPayloadPath(nearby::PayloadId payload_id) {
  std::filesystem::path path =
      nearby::linux::DeviceInfo().GetDownloadPath().value_or(
          std::string(getenv("HOME")).append("Downloads"));

  return path.string();
}

}  // namespace test_utils
