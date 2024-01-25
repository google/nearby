// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_DEF_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_DEF_H_

#ifdef _WIN32  // These storage class specifiers only matter to win32 dll
               // builds.
#ifdef NC_DART_DLL
// If we're building the core, we're exporting.
#define DART_API __declspec(dllexport)
#else  // !NC_DLL
// If we're not building the core, we're importing.
#define DART_API __declspec(dllimport)
#endif          // NC_DLL
#else           // !_WIN32
#define DART_API  // We're not building a win32 dll, leave the source unchanged.
#endif          // _WIN32

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_DEF_H_
