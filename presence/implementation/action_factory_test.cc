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

TEST(ActionFactory, CreateActiveUnlockAction) {
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kActionFieldType,
                             action::kActiveUnlockAction);

  Action action = ActionFactory::CreateAction(data_elements);

  EXPECT_EQ(action.action, 1 << 7);
}

TEST(ActionFactory, CreateContextTimestamp) {
  const std::string kTimestamp = absl::HexStringToBytes("0B");

  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kContextTimestampFieldType,
                             kTimestamp);

  Action action = ActionFactory::CreateAction(data_elements);

  EXPECT_EQ(action.action, 0x0B << 12);
}

TEST(ActionFactory, CreateContextTimestampAndFastPair) {
  const std::string kTimestamp = absl::HexStringToBytes("0B");

  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kContextTimestampFieldType,
                             kTimestamp);
  data_elements.emplace_back(DataElement::kActionFieldType,
                             action::kFastPairAction);

  Action action = ActionFactory::CreateAction(data_elements);

  EXPECT_EQ(action.action, (0x0B << 12) | 0x20);
}

TEST(ActionFactory, DecodeActiveUnlockAction) {
  constexpr Action kAction = {.action = 1 << 7};
  std::vector<DataElement> data_elements;

  ActionFactory::DecodeAction(kAction, data_elements);

  EXPECT_THAT(data_elements,
              ElementsAre(DataElement(DataElement::kActionFieldType,
                                      action::kActiveUnlockAction)));
}

TEST(ActionFactory, DecodeContextTimestampAndFastPair) {
  constexpr Action kAction = {.action = (0x0B << 12) | 0x20};
  std::vector<DataElement> data_elements;

  ActionFactory::DecodeAction(kAction, data_elements);

  EXPECT_THAT(data_elements,
              ElementsAre(DataElement(DataElement::kContextTimestampFieldType,
                                      absl::HexStringToBytes("0B")),
                          DataElement(DataElement::kActionFieldType,
                                      action::kFastPairAction)));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
