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

#ifndef PLATFORM_V2_CONFIG_CONFIG_H_
#define PLATFORM_V2_CONFIG_CONFIG_H_

// Clients can modify this file to customize the Nearby C++ codebase as per
// their particular constraints and environments.

// Note: Every entry in this file should conform to the following format, to
// give precedence to command-line options (-D) that set these symbols:
//
//   #ifndef XXX
//   #define XXX 0/1
//   #endif

#ifndef NEARBY_USE_STD_STRING
#define NEARBY_USE_STD_STRING 0
#endif

#ifndef NEARBY_USE_RTTI
#define NEARBY_USE_RTTI 1
#endif

#endif  // PLATFORM_V2_CONFIG_CONFIG_H_
