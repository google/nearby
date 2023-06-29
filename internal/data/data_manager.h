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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_DATA_DATA_MANAGER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_DATA_DATA_MANAGER_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "internal/data/data_set.h"
#include "internal/data/leveldb_data_set.h"
#include "internal/data/memory_data_set.h"

namespace nearby {
namespace data {

class DataManager {
 public:
  enum class DataStorageType : int { kMemory = 0, kLevelDb = 1 };
  explicit DataManager(DataStorageType data_storage_type)
      : data_storage_type_(data_storage_type) {}
  ~DataManager() = default;

  template <typename T>
  std::unique_ptr<DataSet<T>> GetDataSet(absl::string_view path) {
    if (data_storage_type_ == DataStorageType::kMemory) {
      return std::make_unique<MemoryDataSet<T>>(path);
    } else if (data_storage_type_ == DataStorageType::kLevelDb) {
      return std::make_unique<LeveldbDataSet<T>>(path);
    } else {
      return nullptr;
    }
  }

 private:
  DataStorageType data_storage_type_;
};

}  // namespace data
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_DATA_DATA_MANAGER_H_
