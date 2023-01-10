// Copyright 2021 Google LLC
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
#ifndef CORE_OPTIONS_BASE_H_
#define CORE_OPTIONS_BASE_H_
#include <string>

#include "connections/cpp/medium_selector.h"
#include "connections/cpp/strategy.h"

namespace nearby {
namespace connections {

// Connection Options: used for both Advertising and Discovery.
// All fields are mutable, to make the type copy-assignable.
struct OptionsBase {
  Strategy strategy;
  BooleanMediumSelector allowed{BooleanMediumSelector().SetAll(true)};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_OPTIONS_BASE_H_
