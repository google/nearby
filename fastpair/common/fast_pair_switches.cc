// Copyright 2023 Google LLC
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

#include "fastpair/common/fast_pair_switches.h"

#include <string>

namespace nearby {
namespace fastpair {
namespace switches {

namespace {

// Switch value for nearby fast pair global host.
int global_host_size = 0;
char global_host[256];

}  // namespace

void SetNearbyFastPairHttpHost(const std::string& host) {
  if (host.size() > 256) {
    return;
  }
  global_host_size = host.size();
  memcpy(global_host, host.c_str(), global_host_size);
}

std::string GetNearbyFastPairHttpHost() {
  return std::string(global_host, global_host_size);
}

}  // namespace switches
}  // namespace fastpair
}  // namespace nearby
