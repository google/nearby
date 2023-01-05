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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_MANAGER_MEDIUMS_MANAGER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_MANAGER_MEDIUMS_MANAGER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace api {

// MediumsManager provides methods to access/control platform mediums. In
// general, these APIs need system level permission to make the call, please
// don't add user level platform APIs into this class.
class MediumsManager {
 public:
  virtual ~MediumsManager() = default;

  // Returns true once the BLE advertising has been initiated.
  virtual bool StartBleAdvertising(
      absl::string_view service_id, const ByteArray& advertisement_bytes,
      absl::string_view fast_advertisement_service_uuid) = 0;

  // Returns true once the BLE advertising has been stopped.
  virtual bool StopBleAdvertising(absl::string_view service_id) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_MANAGER_MEDIUMS_MANAGER_H_
