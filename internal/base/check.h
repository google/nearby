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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_CHECK_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_CHECK_H_

#ifdef NEARBY_CHROMIUM
#error "Use chromium headers when NEARBY_CHROMIUM is defined."
#endif

#if defined(NEARBY_SWIFTPM)
// Redirect to nearby logging.
#include "third_party/nearby/internal/platform/logging.h"
#else
// Redirect to absl logging.
#include "absl/log/check.h"
#endif

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_CHECK_H_
