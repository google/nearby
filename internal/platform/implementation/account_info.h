// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ACCOUNT_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ACCOUNT_INFO_H_

#include <string>

namespace nearby {

// Describes a Nearby account. The account class will have more properties
// and methods in the future based on the new feature added.
struct AccountInfo {
  std::string id;  // The unique identify of the account.
  std::string display_name;
  std::string family_name;
  std::string given_name;
  std::string picture_url;
  std::string email;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ACCOUNT_INFO_H_
