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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_FLAGS_FLAG_H_
#define THIRD_PARTY_NEARBY_INTERNAL_FLAGS_FLAG_H_

#include "absl/strings/string_view.h"

namespace nearby {
namespace flags {

template <typename T>
class Flag {
 public:
  constexpr Flag(const absl::string_view config_package_name,
                 const absl::string_view name, const T default_value)
      : config_package_name_(config_package_name),
        name_(name),
        default_value_(default_value) {}

  absl::string_view config_package_name() const { return config_package_name_; }
  absl::string_view name() const { return name_; }
  T default_value() const { return default_value_; }

 private:
  const absl::string_view config_package_name_;
  const absl::string_view name_;
  const T default_value_;
};

}  // namespace flags
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_FLAGS_FLAG_H_
