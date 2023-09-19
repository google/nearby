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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_DATA_LEVELDB_DATA_SET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_DATA_LEVELDB_DATA_SET_H_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "third_party/leveldb/include/db.h"
#include "third_party/leveldb/include/iterator.h"
#include "third_party/leveldb/include/options.h"
#include "third_party/leveldb/include/slice.h"
#include "third_party/leveldb/include/status.h"
#include "internal/data/data_set.h"
#include "internal/platform/logging.h"
#include "third_party/protobuf/message_lite.h"

namespace nearby {
namespace data {
// DataSet implementation using leveldb as its persistent storage. Values are
// serialized and stored in leveldb databases.
template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value,
                           bool> = true>
class LeveldbDataSet : public DataSet<T> {
 public:
  using KeyEntryVector = std::vector<std::pair<std::string, T>>;

  explicit LeveldbDataSet(absl::string_view path) : path_(path) {}
  ~LeveldbDataSet() override = default;

  void Initialize(absl::AnyInvocable<void(InitStatus) &&> callback) override;
  void LoadEntries(
      absl::AnyInvocable<void(bool, std::unique_ptr<std::vector<T>>) &&>
          callback) override;
  void LoadEntriesWithKeys(
      absl::AnyInvocable<
          void(bool,
               std::unique_ptr<std::vector<std::pair<std::string, T>>>) &&>
          callback);
  void UpdateEntries(std::unique_ptr<KeyEntryVector> entries_to_save,
                     std::unique_ptr<std::vector<std::string>> keys_to_remove,
                     absl::AnyInvocable<void(bool) &&> callback) override;
  void Destroy(absl::AnyInvocable<void(bool) &&> callback) override;

 private:
  void Serialize(T const& value, std::string& str);
  void Deserialize(absl::string_view str, T& value);

 private:
  std::string path_;
  std::unique_ptr<leveldb::DB> db_ = nullptr;
  InitStatus status_ = InitStatus::kNotInitialized;
};

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::Initialize(
    absl::AnyInvocable<void(InitStatus) &&> callback) {
  leveldb::Options options;
  options.create_if_missing = true;

  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, path_, &db);
  db_ = std::unique_ptr<leveldb::DB>(db);

  if (status.ok()) {
    status_ = InitStatus::kOK;
    NEARBY_LOGS(INFO) << "Database is initialized successfully..";
  } else if (status.IsCorruption() || status.IsIOError()) {
    status_ = InitStatus::kCorrupt;
    NEARBY_LOGS(INFO) << "Database is corrupt.";

  } else {
    status_ = InitStatus::kError;
    NEARBY_LOGS(INFO) << "Failed to initialize database due to unknown error.";
  }
  std::move(callback)(status_);
}

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::LoadEntries(
    absl::AnyInvocable<void(bool, std::unique_ptr<std::vector<T>>) &&>
        callback) {
  auto result = std::make_unique<std::vector<T>>();
  if (status_ != InitStatus::kOK) {
    std::move(callback)(false, std::move(result));
    return;
  }

  std::unique_ptr<leveldb::Iterator> it(
      db_->NewIterator(leveldb::ReadOptions()));

  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    T value;
    Deserialize(it->value().ToString(), value);
    result->push_back(value);
  }

  if (it->status().ok()) {
    NEARBY_LOGS(INFO) << "Loaded " << result->size()
                      << " entries from database.";
    std::move(callback)(true, std::move(result));
  } else {
    NEARBY_LOGS(INFO) << "Failed to load entries from database.";
    result->clear();
    std::move(callback)(false, std::move(result));
  }
}

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::LoadEntriesWithKeys(
    absl::AnyInvocable<
        void(bool, std::unique_ptr<std::vector<std::pair<std::string, T>>>) &&>
        callback) {
  auto result = std::make_unique<std::vector<std::pair<std::string, T>>>();
  if (status_ != InitStatus::kOK) {
    std::move(callback)(false, std::move(result));
    return;
  }

  std::unique_ptr<leveldb::Iterator> it(
      db_->NewIterator(leveldb::ReadOptions()));

  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    T value;
    Deserialize(it->value().ToString(), value);
    result->push_back({it->key().ToString(), value});
  }

  if (it->status().ok()) {
    NEARBY_LOGS(INFO) << "Loaded " << result->size()
                      << " entries from database.";
    std::move(callback)(true, std::move(result));
  } else {
    NEARBY_LOGS(INFO) << "Failed to load entries from database.";
    result->clear();
    std::move(callback)(false, std::move(result));
  }
}

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::UpdateEntries(
    std::unique_ptr<KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    absl::AnyInvocable<void(bool) &&> callback) {
  NEARBY_LOGS(INFO) << "UpdateEntries is called.";
  if (status_ != InitStatus::kOK) {
    std::move(callback)(false);
    return;
  }

  if (entries_to_save != nullptr) {
    for (const auto& [key, value] : *entries_to_save) {
      std::string str;
      Serialize(value, str);
      db_->Put(leveldb::WriteOptions(), key, leveldb::Slice(str));
    }
  }

  if (keys_to_remove != nullptr) {
    for (const auto& it : *keys_to_remove) {
      db_->Delete(leveldb::WriteOptions(), it);
    }
  }

  std::move(callback)(true);
}

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::Destroy(
    absl::AnyInvocable<void(bool) &&> callback) {
  NEARBY_LOGS(INFO) << "Destroy is called.";
  db_.reset();
  leveldb::DestroyDB(path_, leveldb::Options());
  std::move(callback)(true);
}

// Functions for serializing/deserializing data values to/from strings. Strings
// are used as a convenient container that manages its memory. They don't need
// to be human-readable.

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::Serialize(T const& value,
                                                 std::string& str) {
  value.SerializeToString(&str);
}

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::Deserialize(absl::string_view str,
                                                   T& value) {
  value.ParseFromString(str);
}

}  // namespace data
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_DATA_LEVELDB_DATA_SET_H_
