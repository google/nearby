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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_REGISTRY_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_REGISTRY_H_

#include <cstdint>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/windows/registry_accessor.h"

namespace nearby::platform::windows {

class Registry {
 public:
  enum class Hive {
    kCurrentConfig,
    kCurrentUser,

    // Local machine hives
    // kSAM, Blocked
    // kSecurity, Blocked
    kSoftware,
    kSystem
  };

  enum class Key {
    kClients,            // Read from Google\Update\Clients.
    kClientState,        // Read from Google\Update\ClientState.
    kClientStateMedium,  // Read from Google\Update\ClientStateMedium.
    kNearbyShare,        // Read from Google\NearbyShare.
    kNearbyShareFlags,   // Read from Google\NearbyShare\Flags.
    kWindows  // Read from Microsoft\Windows NT\CurrentVersion, for testing.
  };

  static std::optional<uint32_t> ReadDword(
      Hive hive, Key key, const std::string& value,
      std::optional<absl::string_view> pretty_name = std::nullopt);
  static std::optional<std::string> ReadString(
      Hive hive, Key key, const std::string& value,
      std::optional<absl::string_view> pretty_name = std::nullopt);

  // If |create_sub_key| is true, the sub_key will be created if it does not
  // exist.
  // NOTE: do not set create_sub_key to true unless you know the current user
  // has the permission to create the sub key otherwise elevation will be
  // required.
  static bool SetDword(Hive hive, Key key, const std::string& value,
                       uint32_t data, bool create_sub_key = false);
  // If |create_sub_key| is true, the sub_key will be created if it does not
  // exist.
  // NOTE: do not set create_sub_key to true unless you know the current user
  // has the permission to create the sub key otherwise elevation will be
  // required.
  static bool SetString(Hive hive, Key key, const std::string& value,
                        const std::string& data, bool create_sub_key = false);

  // Enumerate all string and dword values in the registry key.
  static bool EnumValues(
      Hive hive, Key key,
      absl::flat_hash_map<std::string, std::string>& string_values,
      absl::flat_hash_map<std::string, DWORD>& dword_values);

  // Use provided RegistryAccessor for testing.
  static void SetRegistryAccessorForTest(RegistryAccessor* registry_accessor);

  // Set a function that is used to provide a Omaha product Id.
  static void SetProductIdGetter(absl::string_view (*product_id_getter)());
};

}  // namespace nearby::platform::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_REGISTRY_H_
