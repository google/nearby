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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_DATA_DATA_SET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_DATA_DATA_SET_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"

namespace nearby {
namespace data {

enum class InitStatus {
  kOK = 0,
  kNotInitialized = 1,
  kError = 2,
  kCorrupt = 3,
  kInvalidOperation = 4,
  kMaxValue = kInvalidOperation
};

template <typename T>
class DataSet {
 public:
  using KeyEntryVector = std::vector<std::pair<std::string, T>>;

  virtual ~DataSet() = default;

  // Asynchronously initializes the object, which must have been created by the
  // DataManager::GetDataSet<T> function. |callback| will be invoked on the
  // calling thread when complete.
  virtual void Initialize(absl::AnyInvocable<void(InitStatus) &&> callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(
      absl::AnyInvocable<void(bool, std::unique_ptr<std::vector<T>>) &&>
          callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries from
  // |keys_to_remove| from the database. |callback| will be invoked on the
  // calling thread when complete. |entries_to_save| and |keys_to_remove| must
  // be non-null.
  virtual void UpdateEntries(
      std::unique_ptr<KeyEntryVector> entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      absl::AnyInvocable<void(bool) &&> callback) = 0;

  // Asynchronously destroys the database. Use this call only if the database
  // needs to be destroyed for this particular profile.
  virtual void Destroy(absl::AnyInvocable<void(bool) &&> callback) = 0;
};

}  // namespace data
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_DATA_DATA_SET_H_
