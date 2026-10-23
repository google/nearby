// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/nearby_logger.h"

#include <cstdint>
#include <string>

#include "absl/log/log_sink_registry.h"
#include "internal/platform/implementation/apple/os_log_sink.h"

namespace nearby {
namespace apple {
namespace {

OsLogSink *GetOsLogSink(const std::string &subsystem) {
  static OsLogSink *sink = new OsLogSink(subsystem);
  return sink;
}

}  // namespace

void EnableOsLog(const std::string &subsystem) { absl::AddLogSink(GetOsLogSink(subsystem)); }

}  // namespace apple
}  // namespace nearby
