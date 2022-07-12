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

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "third_party/nearby/presence/broadcast_request.h"
#include "third_party/nearby/presence/data_element.h"

namespace nearby {
namespace presence {
namespace {

TEST(ActionFactory, CreateActiveUnlockAction) {
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kActiveUnlock, "");

  Action action = ActionFactory::createAction(data_elements);

  EXPECT_EQ(action.action, 1 << 11);
}

TEST(ActionFactory, CreateContextTimestamp) {
  const std::string kTimestamp = absl::HexStringToBytes("0B");

  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kContextTimestamp, kTimestamp);

  Action action = ActionFactory::createAction(data_elements);

  EXPECT_EQ(action.action, 0x0B << 12);
}

TEST(ActionFactory, CreateContextTimestampAndFastPair) {
  const std::string kTimestamp = absl::HexStringToBytes("0B");

  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kContextTimestamp, kTimestamp);
  data_elements.emplace_back(DataElement::kFastPair, "");

  Action action = ActionFactory::createAction(data_elements);

  EXPECT_EQ(action.action, (0x0B << 12) | 0x100);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
