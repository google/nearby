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

#include "third_party/nearby/sharing/internal/api/platform_factory.h"

#include "absl/base/no_destructor.h"
#include "third_party/nearby/sharing/internal/api/sharing_platform.h"
#include "third_party/nearby/sharing/internal/impl/g3/g3_platform.h"

namespace nearby::sharing::api {
SharingPlatform& GetSharingPlatform() {
  static absl::NoDestructor<nearby::platform::g3::GooglePlatform>
      platform;
  return *platform;
}
}  // namespace nearby::sharing::api
