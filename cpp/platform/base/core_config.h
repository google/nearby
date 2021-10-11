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

#ifdef _WIN32  // These storage class specifiers only matter to win32 dll
               // builds.
#ifdef CORE_ADAPTER_DLL
#define DLL_API \
  __declspec(dllexport)  // If we're building the core, we're exporting.
#else                    // !CORE_ADAPTER_DLL
#define DLL_API \
  __declspec(dllimport)  // If we're not building the core, we're importing.
#endif                   // CORE_ADAPTER_DLL
#else                    // !_WIN32
#define DLL_API  // We're not building a win32 dll, leave the source unchanged.
#endif           // _WIN32

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_CONFIG_H_
