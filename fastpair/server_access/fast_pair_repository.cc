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

#include "fastpair/server_access/fast_pair_repository.h"

#include <utility>

#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace {
FastPairRepository* g_instance = nullptr;
}

FastPairRepository* FastPairRepository::Get() {
  CHECK(g_instance);
  return g_instance;
}

void FastPairRepository::SetInstance(FastPairRepository* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

FastPairRepository::FastPairRepository() { SetInstance(this); }

FastPairRepository::~FastPairRepository() { SetInstance(nullptr); }

}  // namespace fastpair
}  // namespace nearby
