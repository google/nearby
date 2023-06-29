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

#include "internal/data/memory_data_set.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace data {
namespace {

TEST(MemoryDataSet, TestUpdateEntries) {
  bool result = false;
  MemoryDataSet<std::string> string_set{""};

  auto temp = MemoryDataSet<std::string>::KeyEntryVector(
      {{"id1", "string1"}, {"id2", "string2"}});

  auto data =
      std::make_unique<MemoryDataSet<std::string>::KeyEntryVector>(temp);
  string_set.UpdateEntries(std::move(data), nullptr,
                           [&result](bool res) { result = res; });
  EXPECT_TRUE(result);
}

TEST(MemoryDataSet, TestLoadEntries) {
  std::vector<std::string> result = {};
  MemoryDataSet<std::string> string_set{""};

  auto temp = MemoryDataSet<std::string>::KeyEntryVector(
      {{"id1", "string1"}, {"id2", "string2"}});
  auto data =
      std::make_unique<MemoryDataSet<std::string>::KeyEntryVector>(temp);

  string_set.UpdateEntries(std::move(data), nullptr, [](bool ans) {});
  string_set.LoadEntries(
      [&result](bool ans, std::unique_ptr<std::vector<std::string>> res) {
        auto it = res->begin();
        while (it != res->end()) {
          result.push_back(*it);
          ++it;
        }
      });

  EXPECT_THAT(result, testing::SizeIs(2));
  std::sort(result.begin(), result.end());
  EXPECT_EQ(result, std::vector<std::string>({"string1", "string2"}));
}

}  // namespace
}  // namespace data
}  // namespace nearby
