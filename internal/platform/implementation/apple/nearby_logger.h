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

#ifndef PLATFORM_IMPL_APPLE_NEARBY_LOGGER_H_
#define PLATFORM_IMPL_APPLE_NEARBY_LOGGER_H_

#include <string>

namespace nearby {
namespace apple {

// Enables the OS log output as the given subsystem. The method should be called
// once during the application lifetime.
void EnableOsLog(const std::string& subsystem);

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_NEARBY_LOGGER_H_
