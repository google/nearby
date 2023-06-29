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

#include "internal/test/fake_data_set.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/data/data_set.h"

namespace nearby {
namespace data {
namespace {

TEST(FakeDataSet, TestInitialize) {
  InitStatus result = InitStatus::kNotInitialized;
  FakeDataSet<std::string> string_set({});

  string_set.Initialize([&result](InitStatus res) { result = res; });
  string_set.InitStatusCallback(InitStatus::kOK);
  EXPECT_EQ(result, InitStatus::kOK);
}

TEST(FakeDataSet, TestUpdateEntries) {
  bool result = false;
  FakeDataSet<std::string> string_set({});

  auto temp = FakeDataSet<std::string>::KeyEntryVector(
      {{"id1", "string1"}, {"id2", "string2"}});

  auto data = std::make_unique<FakeDataSet<std::string>::KeyEntryVector>(temp);

  string_set.UpdateEntries(std::move(data), nullptr,
                           [&result](bool res) { result = res; });

  string_set.UpdateCallback(true);
  EXPECT_TRUE(result);
  data = std::make_unique<FakeDataSet<std::string>::KeyEntryVector>(temp);
  string_set.UpdateEntries(std::move(data), nullptr,
                           [&result](bool res) { result = res; });
  string_set.UpdateCallback(false);
  EXPECT_FALSE(result);
}

TEST(FakeDataSet, TestLoadEntries) {
  std::vector<std::string> result = {};
  FakeDataSet<std::string> string_set({});

  auto temp = FakeDataSet<std::string>::KeyEntryVector(
      {{"id1", "string1"}, {"id2", "string2"}});
  auto data = std::make_unique<FakeDataSet<std::string>::KeyEntryVector>(temp);

  string_set.UpdateEntries(std::move(data), nullptr, [](bool ans) {});
  string_set.UpdateCallback(true);
  string_set.LoadEntries(
      [&result](bool ans, std::unique_ptr<std::vector<std::string>> res) {
        auto it = res->begin();
        while (it != res->end()) {
          result.push_back(*it);
          ++it;
        }
      });
  string_set.LoadCallback(true);
  EXPECT_THAT(result, testing::SizeIs(2));
  std::sort(result.begin(), result.end());
  EXPECT_EQ(result, std::vector<std::string>({"string1", "string2"}));
}

TEST(MockDataSet, TestDestroy) {
  bool result;
  std::vector<std::string> data = {};
  FakeDataSet<std::string> string_set({{"id1", "string1"}, {"id2", "string2"}});
  string_set.Destroy([&result](bool res) { result = res; });
  string_set.DestroyCallback(true);
  EXPECT_TRUE(result);
  string_set.LoadEntries(
      [&data](bool ans, std::unique_ptr<std::vector<std::string>> res) {
        auto it = res->begin();
        while (it != res->end()) {
          data.push_back(*it);
          ++it;
        }
      });
  string_set.LoadCallback(true);
  EXPECT_THAT(data, ::testing::SizeIs(0));
}

}  // namespace
}  // namespace data
}  // namespace nearby
