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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ACTION_FACTORY_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ACTION_FACTORY_H_

#include <vector>

#include "presence/data_element.h"
#include "presence/implementation/base_broadcast_request.h"

namespace nearby {
namespace presence {

// Defines the mapping between Data Elements and Actions in the Base NP
// advertisement.
class ActionFactory {
 public:
  // Returns an Action for Base NP advertisement from a collection of Data
  // Elements. Data Elements unsupported in the Base NP advertisement are
  // ignored.
  static Action CreateAction(const std::vector<DataElement>& data_elements);

  // Decodes a Base NP Action into a list of Data Elements. The Data Elements
  // are appended to the `output` list.
  //
  // DecodeAction is effectively a reverse operation of CreateAction.
  static void DecodeAction(const Action& action,
                           std::vector<DataElement>& output);
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ACTION_FACTORY_H_
