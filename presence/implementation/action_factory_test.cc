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

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "presence/data_element.h"
#include "presence/implementation/base_broadcast_request.h"

namespace nearby {
namespace presence {
namespace {

using ::testing::ElementsAre;

constexpr uint32_t kActiveUnlockBitMask = 1 << 23;
constexpr uint32_t kFastPairBitMask = 1 << 14;

TEST(ActionFactory, CreateActiveUnlockAction) {
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));

  Action action = ActionFactory::CreateAction(data_elements);

  EXPECT_EQ(action.action, kActiveUnlockBitMask);
}

TEST(ActionFactory, CreateActiveIgnoresUnsupportedActions) {
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  // The action is 32 bit, so the valid range is [0-31]
  data_elements.emplace_back(DataElement(ActionBit(-1)));
  data_elements.emplace_back(DataElement(ActionBit(32)));
  Action action = ActionFactory::CreateAction(data_elements);

  EXPECT_EQ(action.action, kActiveUnlockBitMask);
}

TEST(ActionFactory, CreateContextTimestamp) {
  const std::string kTimestamp = absl::HexStringToBytes("0B");

  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kContextTimestampFieldType,
                             kTimestamp);

  Action action = ActionFactory::CreateAction(data_elements);

  EXPECT_EQ(action.action, 0x0BU << 28);
}

TEST(ActionFactory, CreateContextTimestampAndFastPair) {
  const std::string kTimestamp = absl::HexStringToBytes("0B");

  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kContextTimestampFieldType,
                             kTimestamp);
  data_elements.emplace_back(DataElement(ActionBit::kFastPairAction));

  Action action = ActionFactory::CreateAction(data_elements);

  EXPECT_EQ(action.action, (0x0BU << 28) | kFastPairBitMask);
}

TEST(ActionFactory, DecodeActiveUnlockAction) {
  constexpr Action kAction = {.action = kActiveUnlockBitMask};
  std::vector<DataElement> data_elements;

  ActionFactory::DecodeAction(kAction, data_elements);

  EXPECT_THAT(
      data_elements,
      ElementsAre(DataElement(DataElement(ActionBit::kActiveUnlockAction))));
}

TEST(ActionFactory, DecodeContextTimestampAndFastPair) {
  constexpr Action kAction = {.action = (0x0BU << 28) | kFastPairBitMask};
  std::vector<DataElement> data_elements;

  ActionFactory::DecodeAction(kAction, data_elements);

  EXPECT_THAT(
      data_elements,
      ElementsAre(DataElement(DataElement::kContextTimestampFieldType,
                              absl::HexStringToBytes("0B")),
                  DataElement(DataElement(ActionBit::kFastPairAction))));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
