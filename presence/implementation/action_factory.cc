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
constexpr int kContentTimestampShift = 12;
constexpr int kEmptyMask = 0;

// The values below match the bitmasks in Base NP Intent.
constexpr int kTapToTransferMask = 1 << 11;
constexpr int kActiveUnlockMask = 1 << 7;
constexpr int kNearbyShareMask = 1 << 6;
constexpr int kFastPairMask = 1 << 5;
constexpr int kFitCastMask = 1 << 4;

namespace {
int GetActionMask(int action) {
  switch (action) {
    case action::kActiveUnlockAction:
      return kActiveUnlockMask;
    case action::kTapToTransferAction:
      return kTapToTransferMask;
    case action::kNearbyShareAction:
      return kNearbyShareMask;
    case action::kFastPairAction:
      return kFastPairMask;
    case action::kFitCastAction:
      return kFitCastMask;
  }
  NEARBY_LOG(WARNING, "Unsupported action %d", action);
  return kEmptyMask;
}

// The reverse of `GetActionMask()`
absl::optional<uint8_t> GetActionFromBitMask(int mask) {
  switch (mask) {
    case kTapToTransferMask:
      return action::kTapToTransferAction;
    case kActiveUnlockMask:
      return action::kActiveUnlockAction;
    case kNearbyShareMask:
      return action::kNearbyShareAction;
    case kFastPairMask:
      return action::kFastPairAction;
    case kFitCastMask:
      return action::kFitCastAction;
    default:
      NEARBY_LOG(WARNING, "Unsupported action for bit mask 0x%x", mask);
      return absl::nullopt;
  }
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
      return GetActionMask(element.GetValue()[0]);
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
  for (int i = 0; i < kContentTimestampShift; i++) {
    int bit_mask = 1 << i;
    if (action.action & bit_mask) {
      absl::optional<uint8_t> action_value = GetActionFromBitMask(bit_mask);
      if (action_value) {
        output.emplace_back(DataElement::kActionFieldType, *action_value);
      }
    }
  }
}

}  // namespace presence
}  // namespace nearby
