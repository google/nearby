// Copyright 2021 Google LLC
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

#include "sharing/scheduling/format.h"

#include <string>

#include "absl/strings/str_format.h"
#include "absl/time/time.h"

namespace nearby {
namespace utils {

std::string TimeFormatShortDateAndTimeWithTimeZone(absl::Time time) {
  absl::TimeZone tz = absl::LocalTimeZone();
  return absl::FormatTime("%Y/%m/%d %H:%M:%S %z", time, tz);
}

std::string TimeDurationFormatWithSeconds(absl::Duration duration) {
  return absl::StrFormat("%ds", duration / absl::Seconds(1));
}

}  // namespace utils
}  // namespace nearby
