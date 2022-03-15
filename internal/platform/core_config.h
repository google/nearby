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
#ifndef CORE_CONFIG_H_
#define CORE_CONFIG_H_

namespace location {
namespace nearby {
namespace connections {

// TODO(b/224783653): Update DLL_API to check if we can add dllimport back.
#define DLL_API  // Define DLL_API as blank since we don't import/export
                 // anything internally.

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_CONFIG_H_
