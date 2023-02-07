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

#ifndef PLATFORM_OS_NAME_H_
#define PLATFORM_OS_NAME_H_

namespace nearby {
namespace api {

enum class OSName { kLinux, kWindows, kApple, kChromeOS, kAndroid };

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_OS_NAME_H_
