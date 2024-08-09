// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_FLAGS_NEARBY_FLAGS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_FLAGS_NEARBY_FLAGS_H_

#include <cstdint>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/flags/default_flag_reader.h"
#include "internal/flags/flag.h"
#include "internal/flags/flag_reader.h"

namespace nearby {

class NearbyFlags final : public nearby::flags::FlagReader {
 public:
  ~NearbyFlags() override = default;

  static NearbyFlags& GetInstance();

  // Reads flag with boolean value.
  bool GetBoolFlag(const flags::Flag<bool>& flag) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Reads flag with int64_t value.
  int64_t GetInt64Flag(const flags::Flag<int64_t>& flag) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Reads flag with double value.
  double GetDoubleFlag(const flags::Flag<double>& flag) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Reads flag with string value.
  std::string GetStringFlag(const flags::Flag<absl::string_view>& flag) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  void SetFlagReader(flags::FlagReader& flag_reader)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Override the default value of the flags. The major purpose of the method is
  // for test.
  void OverrideBoolFlagValue(const flags::Flag<bool>& flag, bool new_value)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void OverrideInt64FlagValue(const flags::Flag<int64_t>& flag,
                              int64_t new_value) ABSL_LOCKS_EXCLUDED(mutex_);

  void OverrideDoubleFlagValue(const flags::Flag<double>& flag,
                               double new_value) ABSL_LOCKS_EXCLUDED(mutex_);

  void OverrideStringFlagValue(const flags::Flag<absl::string_view>& flag,
                               absl::string_view new_value)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Reset all overridden values.
  void ResetOverridedValues() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  NearbyFlags() = default;

  flags::FlagReader* flag_reader_ = nullptr;
  flags::DefaultFlagReader default_flag_reader_;

  mutable absl::Mutex mutex_;
  absl::flat_hash_map<std::string, bool> overrided_bool_flag_values_
      ABSL_GUARDED_BY(mutex_);

  absl::flat_hash_map<std::string, int64_t> overrided_int64_flag_values_
      ABSL_GUARDED_BY(mutex_);

  absl::flat_hash_map<std::string, double> overrided_double_flag_values_
      ABSL_GUARDED_BY(mutex_);

  absl::flat_hash_map<std::string, std::string> overrided_string_flag_values_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_FLAGS_NEARBY_FLAGS_H_
