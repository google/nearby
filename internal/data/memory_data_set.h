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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_DATA_MEMORY_DATA_SET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_DATA_MEMORY_DATA_SET_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/data/data_set.h"

namespace nearby {
namespace data {

template <typename T>
class MemoryDataSet : public DataSet<T> {
 public:
  using KeyEntryVector = std::vector<std::pair<std::string, T>>;

  explicit MemoryDataSet(absl::string_view path) : path_(path) {}
  ~MemoryDataSet() override = default;

  void Initialize(absl::AnyInvocable<void(InitStatus) &&> callback) override;
  void LoadEntries(
      absl::AnyInvocable<void(bool, std::unique_ptr<std::vector<T>>) &&>
          callback) override;
  void UpdateEntries(std::unique_ptr<KeyEntryVector> entries_to_save,
                     std::unique_ptr<std::vector<std::string>> keys_to_remove,
                     absl::AnyInvocable<void(bool) &&> callback) override;
  void Destroy(absl::AnyInvocable<void(bool) &&> callback) override;

 private:
  std::string path_;

  absl::Mutex mutex_;
  absl::flat_hash_map<std::string, T> entries_;
};

template <typename T>
void MemoryDataSet<T>::Initialize(
    absl::AnyInvocable<void(InitStatus) &&> callback) {
  std::move(callback)(InitStatus::kOK);
}

template <typename T>
void MemoryDataSet<T>::LoadEntries(
    absl::AnyInvocable<void(bool, std::unique_ptr<std::vector<T>>) &&>
        callback) {
  auto result = std::make_unique<std::vector<T>>();
  auto it = entries_.begin();
  while (it != entries_.end()) {
    result->push_back(it->second);
    ++it;
  }

  std::move(callback)(true, std::move(result));
}

template <typename T>
void MemoryDataSet<T>::UpdateEntries(
    std::unique_ptr<KeyEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    absl::AnyInvocable<void(bool) &&> callback) {
  if (entries_to_save != nullptr) {
    auto it = entries_to_save->begin();
    while (it != entries_to_save->end()) {
      entries_.emplace(it->first, it->second);
      ++it;
    }
  }

  if (keys_to_remove != nullptr) {
    auto it = keys_to_remove->begin();
    while (it != keys_to_remove->end()) {
      entries_.erase(*it);
      ++it;
    }
  }

  std::move(callback)(true);
}

template <typename T>
void MemoryDataSet<T>::Destroy(absl::AnyInvocable<void(bool) &&> callback) {
  entries_.clear();
  std::move(callback)(true);
}

}  // namespace data
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_DATA_MEMORY_DATA_SET_H_
