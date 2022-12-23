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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_CLIENT_WINDOWS_FAST_PAIR_CONTROLLER_ADAPTER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_CLIENT_WINDOWS_FAST_PAIR_CONTROLLER_ADAPTER_H_

#define DLL_EXPORT extern "C" __declspec(dllexport)

#include "fastpair/fast_pair_controller.h"

namespace location {
namespace nearby {
namespace fastpair {

// Initiate a default FastPairController instance.
// Return the instance handle to client
DLL_EXPORT FastPairController *__stdcall InitFastPairController();

DLL_EXPORT void __stdcall CloseFastPairController(
    FastPairController *pController);

DLL_EXPORT void __stdcall StartScanning(FastPairController *pController);

DLL_EXPORT void __stdcall ServerAccess(FastPairController *pController);

}  // namespace fastpair
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_CLIENT_WINDOWS_FAST_PAIR_CONTROLLER_ADAPTER_H_
