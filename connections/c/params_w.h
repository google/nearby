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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_PARAMS_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_PARAMS_W_H_

#include <string>

#include "connections/c/listeners_w.h"

namespace nearby::windows {

extern "C" {

// Used by Discovery in Core::RequestConnection().
// Used by Advertising in Core::StartAdvertising().
struct DLL_API ConnectionRequestInfoW {
  // endpoint_info - Identifying information about this endpoint (eg. name,
  //                 device type).
  // listener      - A set of callbacks notified when remote endpoints request a
  //                 connection to this endpoint.
  // ByteArray endpoint_info;
  const char* endpoint_info;
  size_t endpoint_info_size;
  ConnectionListenerW& listener;
};

}  // extern "C"
}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_PARAMS_W_H_
