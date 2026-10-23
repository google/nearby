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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_REGISTRY_ACCESSOR_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_REGISTRY_ACCESSOR_H_

#define _WIN32_WINNT _WIN32_WINNT_WIN10

#include <windows.h>

#include <string>

#include "absl/container/flat_hash_map.h"

namespace nearby::platform::windows {

// Interface wrapping windows registry access to facilitate testing.
class RegistryAccessor {
 public:
  virtual ~RegistryAccessor() = default;

  virtual LSTATUS ReadDWordValue(HKEY key, const std::string& sub_key,
                                 const std::string& value_name,
                                 DWORD& value_data) = 0;
  virtual LSTATUS ReadStringValue(HKEY key, const std::string& sub_key,
                                  const std::string& value_name,
                                  std::string& value_data) = 0;
  // If |create_sub_key| is true, the sub_key will be created if it does not
  // exist.
  virtual LSTATUS WriteDWordValue(HKEY key, const std::string& sub_key,
                                  const std::string& value_name,
                                  DWORD value_data, bool create_sub_key) = 0;
  // If |create_sub_key| is true, the sub_key will be created if it does not
  // exist.
  virtual LSTATUS WriteStringValue(HKEY key, const std::string& sub_key,
                                   const std::string& value_name,
                                   const std::string& value_data,
                                   bool create_sub_key) = 0;

  virtual LSTATUS EnumValues(
      HKEY key, const std::string& sub_key,
      absl::flat_hash_map<std::string, std::string>& string_values,
      absl::flat_hash_map<std::string, DWORD>& dword_values) = 0;
};

}  // namespace nearby::platform::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_REGISTRY_ACCESSOR_H_
