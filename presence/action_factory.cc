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

#include "third_party/nearby/presence/action_factory.h"

#include <algorithm>
#include <vector>

#include "internal/platform/logging.h"

namespace nearby {
namespace presence {

constexpr int kContentTimestampMask = 0x0F;
constexpr int kContentTimestampShift = 12;
constexpr int kEmptyMask = 0;

int ActionFactory::GetMask(const DataElement& element) {
  int type = element.GetType();
  switch (type) {
    case DataElement::kContextTimestamp: {
      auto value = element.GetValue();
      if (!value.empty()) {
        return (value[0] & kContentTimestampMask) << kContentTimestampShift;
      } else {
        NEARBY_LOG(WARNING, "Context timestamp Data Element without value");
        return kEmptyMask;
      }
    }
    case DataElement::kActiveUnlock:
    case DataElement::kTapToTransfer:
    case DataElement::kNearbyShare:
    case DataElement::kFastPair:
    case DataElement::kFitCast:
    case DataElement::kPresenceManager:
      return type;
  }
  NEARBY_LOG(WARNING, "Data Element 0x%x not supported in base advertisement",
             type);
  return kEmptyMask;
}

Action ActionFactory::createAction(
    const std::vector<DataElement>& data_elements) {
  Action action = {.action = 0};
  std::for_each(data_elements.begin(), data_elements.end(),
                [&](const auto& element) {
                  int mask = GetMask(element);
                  action.action |= mask;
                });
  return action;
}

}  // namespace presence
}  // namespace nearby
