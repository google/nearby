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
#include "internal/base/file_path.h"
#include "internal/data/data_set.h"
#if defined(_WIN32)
#include "location/nearby/apps/better_together/windows/common/leveldb_env_windows.h"
#endif  // defined(_WIN32)
#include "internal/platform/logging.h"
#include "google/protobuf/message_lite.h"

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

  explicit LeveldbDataSet(const FilePath& db_dir) : path_(db_dir.ToString()) {
#if defined(_WIN32)
    // In Windows use the Unicode compatible environment.
    db_options_.env = nearby::windows::WindowsEnv::Default();
#endif  // defined(_WIN32)
    db_options_.create_if_missing = true;
  }
  ~LeveldbDataSet() override = default;

  void Initialize(absl::AnyInvocable<void(InitStatus) &&> callback) override;
  void LoadEntry(
      absl::string_view key,
      absl::AnyInvocable<void(bool, std::unique_ptr<T>) &&> callback) override;
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
  leveldb::Options db_options_;
  std::unique_ptr<leveldb::DB> db_ = nullptr;
  InitStatus status_ = InitStatus::kNotInitialized;
};

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::Initialize(
    absl::AnyInvocable<void(InitStatus) &&> callback) {
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(db_options_, path_, &db);
  db_ = std::unique_ptr<leveldb::DB>(db);

  if (status.ok()) {
    status_ = InitStatus::kOK;
    LOG(INFO) << "Database is initialized successfully..";
  } else if (status.IsCorruption() || status.IsIOError()) {
    status_ = InitStatus::kCorrupt;
    LOG(WARNING) << "Database is corrupt.";

  } else {
    status_ = InitStatus::kError;
    LOG(ERROR) << "Failed to initialize database due to unknown error.";
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
    VLOG(1) << "Loaded " << result->size() << " entries from database.";
    std::move(callback)(true, std::move(result));
  } else {
    LOG(ERROR) << "Failed to load entries from database.";
    result->clear();
    std::move(callback)(false, std::move(result));
  }
}

template <typename T,
          std::enable_if_t<std::is_base_of<proto2::MessageLite, T>::value, bool>
              isMessageLite>
void LeveldbDataSet<T, isMessageLite>::LoadEntry(
    absl::string_view key,
    absl::AnyInvocable<void(bool, std::unique_ptr<T>) &&> callback) {
  auto result = std::make_unique<T>();
  if (status_ != InitStatus::kOK) {
    std::move(callback)(false, std::move(result));
    return;
  }

  std::string value;
  if (!db_->Get(leveldb::ReadOptions(), std::string(key), &value).ok()) {
    LOG(WARNING) << "Failed to load entry from database with key: " << key;
    std::move(callback)(false, std::move(result));
    return;
  }
  Deserialize(value, *result);
  std::move(callback)(true, std::move(result));
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
    VLOG(1) << "Loaded " << result->size() << " entries from database.";
    std::move(callback)(true, std::move(result));
  } else {
    LOG(WARNING) << "Failed to load entries from database.";
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
  VLOG(1) << "UpdateEntries is called.";
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
  VLOG(1) << "Destroy is called.";
  db_.reset();
  leveldb::DestroyDB(path_, db_options_);
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
