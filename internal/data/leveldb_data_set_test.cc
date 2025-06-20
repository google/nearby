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

#include "internal/data/leveldb_data_set.h"

#include <stdint.h>

#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/data/data_set.h"
#include "internal/data/leveldb_data_set_test.proto.h"

namespace nearby::data {
namespace {

using ::testing::SizeIs;

// Generate a unique directory under temp directory for leveldb storage
FilePath GenerateLeveldbPath() {
  FilePath temp_directory_path = Files::GetTemporaryDirectory();
  std::random_device dev;
  std::mt19937 prng(dev());
  std::uniform_int_distribution<uint64_t> rand(0);
  FilePath path;
  do {
    path = temp_directory_path;
    path.append(FilePath(absl::StrCat("nearby_db_", absl::Hex(rand(prng)))));
  } while (Files::FileExists(path));
  return path;
}

// Helper functions to synchronize LeveldbDataSet function calls for testing
template <typename T>
std::unique_ptr<LeveldbDataSet<T>> CreateDataSet(
    const FilePath& path) {
  return std::make_unique<LeveldbDataSet<T>>(path);
}

template <typename T>
InitStatus InitializeAndWait(std::unique_ptr<LeveldbDataSet<T>>& dataset) {
  InitStatus status;
  absl::Notification notification;
  dataset->Initialize([&notification, &status](InitStatus s) {
    status = s;
    notification.Notify();
  });
  notification.WaitForNotificationWithTimeout(absl::Seconds(5));
  return status;
}

template <typename T>
bool UpdateEntriesAndWait(
    std::unique_ptr<LeveldbDataSet<T>>& dataset,
    std::unique_ptr<typename LeveldbDataSet<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove) {
  bool result = false;
  absl::Notification notification;
  dataset->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                         [&result, &notification](bool res) {
                           result = res;
                           notification.Notify();
                         });
  notification.WaitForNotificationWithTimeout(absl::Seconds(5));
  return result;
}

template <typename T>
std::unique_ptr<std::vector<T>> LoadEntriesAndWait(
    std::unique_ptr<LeveldbDataSet<T>>& dataset) {
  auto result = std::make_unique<std::vector<T>>();
  absl::Notification notification;
  dataset->LoadEntries(
      [&result, &notification](bool, std::unique_ptr<std::vector<T>> res) {
        for (const auto& it : *res) {
          result->push_back(it);
        }
        notification.Notify();
      });
  notification.WaitForNotificationWithTimeout(absl::Seconds(5));
  return result;
}

template <typename T>
absl::flat_hash_map<std::string, T> LoadEntriesWithKeysAndWait(
    std::unique_ptr<LeveldbDataSet<T>>& dataset) {
  auto result = std::make_unique<std::vector<std::pair<std::string, T>>>();
  absl::Notification notification;
  dataset->LoadEntriesWithKeys(
      [&result, &notification](
          bool, std::unique_ptr<std::vector<std::pair<std::string, T>>> res) {
        for (const auto& it : *res) {
          result->push_back(it);
        }
        notification.Notify();
      });
  notification.WaitForNotificationWithTimeout(absl::Seconds(5));

  absl::flat_hash_map<std::string, T> entry_map;
  for (const auto& it : *result) {
    entry_map[it.first] = it.second;
  }
  return entry_map;
}

template <typename T>
void WipeCleanAndWait(std::unique_ptr<LeveldbDataSet<T>>& dataset,
                      FilePath path) {
  absl::Notification notification;
  dataset->Destroy([&notification](bool) { notification.Notify(); });
  notification.WaitForNotificationWithTimeout(absl::Seconds(5));
  // Call the destructor before removing leveldb storage directory
  dataset.reset();
  Files::RemoveDirectory(path);
}

DiceRoll GenerateDiceRoll(int value) {
  DiceRoll result;
  result.set_value(value);
  if (value == 12) {
    result.set_nickname("boxcars");
  }

  if (value == 2) {
    result.set_nickname("snake eyes");
  }

  return result;
}

TEST(LeveldbDataSet, UpdateEntriesDiceRoll) {
  FilePath path = GenerateLeveldbPath();
  std::unique_ptr<LeveldbDataSet<DiceRoll>> diceroll_set =
      CreateDataSet<DiceRoll>(path);

  InitStatus status = InitializeAndWait(diceroll_set);
  ASSERT_EQ(status, InitStatus::kOK);

  DiceRoll diceroll1 = GenerateDiceRoll(2);
  DiceRoll diceroll2 = GenerateDiceRoll(12);

  auto entries = LeveldbDataSet<DiceRoll>::KeyEntryVector(
      {{"id1", diceroll1}, {"id2", diceroll2}});
  auto data =
      std::make_unique<LeveldbDataSet<DiceRoll>::KeyEntryVector>(entries);

  bool result = UpdateEntriesAndWait(diceroll_set, std::move(data), nullptr);
  WipeCleanAndWait(diceroll_set, path);

  EXPECT_TRUE(result);
}

TEST(LeveldbDataSet, LoadEntriesDiceRoll) {
  FilePath path = GenerateLeveldbPath();
  std::unique_ptr<LeveldbDataSet<DiceRoll>> diceroll_set =
      CreateDataSet<DiceRoll>(path);

  InitializeAndWait(diceroll_set);

  DiceRoll diceroll1 = GenerateDiceRoll(2);
  DiceRoll diceroll2 = GenerateDiceRoll(12);

  auto entries = LeveldbDataSet<DiceRoll>::KeyEntryVector(
      {{"id1", diceroll1}, {"id2", diceroll2}});
  auto data =
      std::make_unique<LeveldbDataSet<DiceRoll>::KeyEntryVector>(entries);
  UpdateEntriesAndWait(diceroll_set, std::move(data), nullptr);

  auto result = LoadEntriesAndWait(diceroll_set);
  WipeCleanAndWait(diceroll_set, path);

  EXPECT_THAT(*result, SizeIs(2));

  // EqualsProto is only available internally
  // EXPECT_THAT((*result)[0], protobuf_matchers::EqualsProto<DiceRoll>(
  //                               "value: 2 nickname: 'snake eyes'"));
  // EXPECT_THAT((*result)[1],
  //             protobuf_matchers::EqualsProto<DiceRoll>("value: 12 nickname:
  //             'boxcars'"));

  EXPECT_EQ((*result)[0].value(), 2);
  EXPECT_EQ((*result)[0].nickname(), "snake eyes");

  EXPECT_EQ((*result)[1].value(), 12);
  EXPECT_EQ((*result)[1].nickname(), "boxcars");
}

TEST(LeveldbDataSet, LoadEntrysDiceRoll) {
  FilePath path = GenerateLeveldbPath();
  std::unique_ptr<LeveldbDataSet<DiceRoll>> diceroll_set =
      CreateDataSet<DiceRoll>(path);

  InitializeAndWait(diceroll_set);

  DiceRoll diceroll1 = GenerateDiceRoll(2);
  DiceRoll diceroll2 = GenerateDiceRoll(12);

  auto entries = LeveldbDataSet<DiceRoll>::KeyEntryVector(
      {{"id1", diceroll1}, {"id2", diceroll2}});
  auto data =
      std::make_unique<LeveldbDataSet<DiceRoll>::KeyEntryVector>(entries);
  UpdateEntriesAndWait(diceroll_set, std::move(data), nullptr);

  absl::Notification notification;
  std::unique_ptr<DiceRoll> result;
  diceroll_set->LoadEntry(
      "id2", [&result, &notification](bool, std::unique_ptr<DiceRoll> res) {
        result = std::move(res);
        notification.Notify();
      });
  notification.WaitForNotificationWithTimeout(absl::Seconds(5));
  WipeCleanAndWait(diceroll_set, path);

  EXPECT_THAT(*result,
              protobuf_matchers::EqualsProto<DiceRoll>("value: 12, nickname:'boxcars'"));
}

TEST(LeveldbDataSet, RemoveEntriesDiceRoll) {
  FilePath path = GenerateLeveldbPath();
  std::unique_ptr<LeveldbDataSet<DiceRoll>> diceroll_set =
      CreateDataSet<DiceRoll>(path);

  InitializeAndWait(diceroll_set);

  DiceRoll diceroll1 = GenerateDiceRoll(2);
  DiceRoll diceroll2 = GenerateDiceRoll(12);
  DiceRoll diceroll3 = GenerateDiceRoll(5);
  DiceRoll diceroll4 = GenerateDiceRoll(7);

  auto entries_to_add1 = LeveldbDataSet<DiceRoll>::KeyEntryVector(
      {{"id1", diceroll1}, {"id2", diceroll2}});
  auto data_to_add1 =
      std::make_unique<LeveldbDataSet<DiceRoll>::KeyEntryVector>(
          entries_to_add1);
  UpdateEntriesAndWait(diceroll_set, std::move(data_to_add1), nullptr);

  auto entries_to_add2 = LeveldbDataSet<DiceRoll>::KeyEntryVector(
      {{"id3", diceroll3}, {"id4", diceroll4}});
  auto data_to_add2 =
      std::make_unique<LeveldbDataSet<DiceRoll>::KeyEntryVector>(
          entries_to_add2);

  auto keys_to_remove = std::make_unique<std::vector<std::string>>(
      std::vector<std::string>({"id1"}));

  UpdateEntriesAndWait(diceroll_set, std::move(data_to_add2),
                       std::move(keys_to_remove));

  auto result = LoadEntriesWithKeysAndWait(diceroll_set);
  WipeCleanAndWait(diceroll_set, path);

  EXPECT_THAT(result, SizeIs(3));

  EXPECT_EQ(result["id2"].value(), diceroll2.value());
  EXPECT_EQ(result["id2"].nickname(), diceroll2.nickname());
  EXPECT_EQ(result["id3"].value(), diceroll3.value());
  EXPECT_EQ(result["id3"].nickname(), diceroll3.nickname());
  EXPECT_EQ(result["id4"].value(), diceroll4.value());
  EXPECT_EQ(result["id4"].nickname(), diceroll4.nickname());
}

}  // namespace
}  // namespace nearby::data
