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

#include "internal/flags/nearby_flags.h"

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/flags/flag.h"
#include "internal/flags/flag_reader.h"

namespace nearby {

NearbyFlags& NearbyFlags::GetInstance() {
  static NearbyFlags* sharing_flags = new NearbyFlags();
  return *sharing_flags;
}

bool NearbyFlags::GetBoolFlag(const flags::Flag<bool>& flag) {
  absl::MutexLock lock(&mutex_);

  const auto& it = overrided_bool_flag_values_.find(flag.name());
  if (it != overrided_bool_flag_values_.end()) {
    return it->second;
  }

  if (flag_reader_ != nullptr) {
    return flag_reader_->GetBoolFlag(flag);
  }
  return default_flag_reader_.GetBoolFlag(flag);
}

int64_t NearbyFlags::GetInt64Flag(const flags::Flag<int64_t>& flag) {
  absl::MutexLock lock(&mutex_);

  const auto& it = overrided_int64_flag_values_.find(flag.name());
  if (it != overrided_int64_flag_values_.end()) {
    return it->second;
  }

  if (flag_reader_ != nullptr) {
    return flag_reader_->GetInt64Flag(flag);
  }
  return default_flag_reader_.GetInt64Flag(flag);
}

double NearbyFlags::GetDoubleFlag(const flags::Flag<double>& flag) {
  absl::MutexLock lock(&mutex_);

  const auto& it = overrided_double_flag_values_.find(flag.name());
  if (it != overrided_double_flag_values_.end()) {
    return it->second;
  }

  if (flag_reader_ != nullptr) {
    return flag_reader_->GetDoubleFlag(flag);
  }
  return default_flag_reader_.GetDoubleFlag(flag);
}

std::string NearbyFlags::GetStringFlag(
    const flags::Flag<absl::string_view>& flag) {
  absl::MutexLock lock(&mutex_);

  const auto& it = overrided_string_flag_values_.find(flag.name());
  if (it != overrided_string_flag_values_.end()) {
    return it->second;
  }

  if (flag_reader_ != nullptr) {
    return flag_reader_->GetStringFlag(flag);
  }
  return default_flag_reader_.GetStringFlag(flag);
}

void NearbyFlags::SetFlagReader(flags::FlagReader& flag_reader) {
  absl::MutexLock lock(&mutex_);
  flag_reader_ = &flag_reader;
}

void NearbyFlags::OverrideBoolFlagValue(const flags::Flag<bool>& flag,
                                        bool new_value) {
  absl::MutexLock lock(&mutex_);
  overrided_bool_flag_values_[flag.name()] = new_value;
}

void NearbyFlags::OverrideInt64FlagValue(const flags::Flag<int64_t>& flag,
                                         int64_t new_value) {
  absl::MutexLock lock(&mutex_);
  overrided_int64_flag_values_[flag.name()] = new_value;
}

void NearbyFlags::OverrideDoubleFlagValue(const flags::Flag<double>& flag,
                                          double new_value) {
  absl::MutexLock lock(&mutex_);
  overrided_double_flag_values_[flag.name()] = new_value;
}

void NearbyFlags::OverrideStringFlagValue(
    const flags::Flag<absl::string_view>& flag, absl::string_view new_value) {
  absl::MutexLock lock(&mutex_);
  overrided_string_flag_values_[flag.name()] = std::string(new_value);
}

void NearbyFlags::ResetOverridedValues() {
  absl::MutexLock lock(&mutex_);
  overrided_bool_flag_values_.clear();
  overrided_int64_flag_values_.clear();
  overrided_double_flag_values_.clear();
  overrided_string_flag_values_.clear();
}

}  // namespace nearby
