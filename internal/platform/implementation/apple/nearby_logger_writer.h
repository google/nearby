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

#ifndef PLATFORM_IMPL_APPLE_NEARBY_LOGGER_WRITER_H_
#define PLATFORM_IMPL_APPLE_NEARBY_LOGGER_WRITER_H_

namespace nearby {
namespace apple {

// NearbyLoggerWriter will handle GTM logs as ABSL logs.
// The SDK developer can setup ABSL log listener to receive all logs from Nearby
// connections.
void EnableNearbyLoggerWriter();

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_NEARBY_LOGGER_WRITER_H_
