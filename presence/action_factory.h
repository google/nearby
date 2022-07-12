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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_ACTION_FACTORY_H_
#define THIRD_PARTY_NEARBY_PRESENCE_ACTION_FACTORY_H_

#include <vector>

#include "third_party/nearby/presence/broadcast_request.h"
#include "third_party/nearby/presence/data_element.h"

namespace nearby {
namespace presence {

/** Defines the mapping between Data Elements and Actions in the Base NP
 * advertisement. */
class ActionFactory {
 public:
  /** Returns an Action for Base NP advertisement from a collection of Data
   * Elements. Data Elements unsupported in the Base NP advertisement are
   * ignored.
   */
  static Action createAction(const std::vector<DataElement>& data_elements);

 private:
  static int GetMask(const DataElement& element);
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ACTION_FACTORY_H_
