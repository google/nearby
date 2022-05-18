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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_OPTIONS_BASE_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_OPTIONS_BASE_W_H_

#include "connections/clients/windows/medium_selector_w.h"
#include "connections/clients/windows/strategy_w.h"

namespace location::nearby::windows {

extern "C" {

// Connection Options: used for both Advertising and Discovery.
// All fields are mutable, to make the type copy-assignable.
struct OptionsBaseW {
  location::nearby::windows::StrategyW strategy;
  BooleanMediumSelectorW allowed{BooleanMediumSelectorW().SetAll(true)};
};

}  // extern "C"
}  // namespace location::nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_OPTIONS_BASE_W_H_
