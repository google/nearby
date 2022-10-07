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

#include "presence/implementation/action_factory.h"

#include <algorithm>
#include <vector>

#include "absl/types/optional.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace presence {

constexpr int kContentTimestampMask = 0x0F;
constexpr int kContentTimestampShift = 28;
constexpr int kEmptyMask = 0;
constexpr int kActionSizeInBits = 32;

namespace {

int GetActionMask(ActionBit action) {
  int bit = static_cast<int>(action);
  if (bit < 0 || bit >= kActionSizeInBits) {
    NEARBY_LOG(WARNING, "Unsupported action %d", static_cast<int>(action));
    return kEmptyMask;
  }
  return 1 << (kActionSizeInBits - 1 - bit);
}

// The reverse of `GetActionMask()`
ActionBit GetActionFromBit(int bit) {
  return ActionBit(kActionSizeInBits - 1 - bit);
}

int GetMask(const DataElement& element) {
  int type = element.GetType();
  switch (type) {
    case DataElement::kContextTimestampFieldType: {
      auto value = element.GetValue();
      if (!value.empty()) {
        return (value[0] & kContentTimestampMask) << kContentTimestampShift;
      } else {
        NEARBY_LOG(WARNING, "Context timestamp Data Element without value");
        return kEmptyMask;
      }
    }
    case DataElement::kActionFieldType: {
      if (element.GetValue().empty()) {
        NEARBY_LOG(WARNING, "Action Data Element without value");
        return kEmptyMask;
      }
      return GetActionMask(ActionBit(element.GetValue()[0]));
    }
  }
  NEARBY_LOG(WARNING, "Data Element 0x%x not supported in base advertisement",
             type);
  return kEmptyMask;
}

}  // namespace

Action ActionFactory::CreateAction(
    const std::vector<DataElement>& data_elements) {
  Action action = {.action = 0};
  std::for_each(data_elements.begin(), data_elements.end(),
                [&](const auto& element) {
                  int mask = GetMask(element);
                  action.action |= mask;
                });
  return action;
}

void ActionFactory::DecodeAction(const Action& action,
                                 std::vector<DataElement>& output) {
  uint8_t context_timestamp =
      (action.action >> kContentTimestampShift) & kContentTimestampMask;
  if (context_timestamp) {
    output.emplace_back(DataElement::kContextTimestampFieldType,
                        context_timestamp);
  }
  constexpr int kFirstUsedBit =
      kActionSizeInBits - static_cast<int>(ActionBit::kLastAction);
  for (int i = kFirstUsedBit; i < kContentTimestampShift; i++) {
    int bit_mask = 1 << i;
    if (action.action & bit_mask) {
      output.emplace_back(DataElement(GetActionFromBit(i)));
    }
  }
}

}  // namespace presence
}  // namespace nearby
