// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_
#define CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_

#include "platform/base/exception.h"
#include "proto/connections/offline_wire_formats.pb.h"

namespace location {
namespace nearby {
namespace connections {
namespace parser {

Exception EnsureValidOfflineFrame(const OfflineFrame& offline_frame);

}  // namespace parser
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_OFFLINE_FRAMES_VALIDATOR_H_
