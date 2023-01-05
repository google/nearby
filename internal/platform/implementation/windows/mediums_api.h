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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_API_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_API_H_

#include <string>
#include <memory>
#include <utility>

#include "internal/platform/implementation/windows/mediums_manager/mediums_manager.h"

namespace nearby {
namespace api {

class MediumsApi {
 public:
  // Creates a new instance of Mediums Manager.
  static std::unique_ptr<MediumsManager> CreateMediumsManager();
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_MEDIUMS_API_H_
