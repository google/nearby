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

#include "fastpair/fast_pair_controller.h"

#include <string>

#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace {

using StatusCodes = FastPairController::StatusCodes;
constexpr char kUnknownStatusCodesString[] = "Unknown_StatusCodes";

}  // namespace

std::string FastPairController::StatusCodeToString(StatusCodes status_code) {
  switch (status_code) {
    case StatusCodes::kOk:
      return "kOk";
    case StatusCodes::kError:
      return "kError";
  }
  NEARBY_LOGS(ERROR) << "Unknown value for Status codes: "
                     << static_cast<int>(status_code);
  return kUnknownStatusCodesString;
}

}  // namespace fastpair
}  // namespace nearby
