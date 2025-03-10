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

#include "sharing/nearby_sharing_service.h"

#include <string>

#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {

namespace {

using StatusCodes = NearbySharingService::StatusCodes;
constexpr char kUnknownStatusCodesString[] = "Unknown_StatusCodes";

}  // namespace

// static
std::string NearbySharingService::StatusCodeToString(StatusCodes status_code) {
  switch (status_code) {
    case StatusCodes::kOk:
      return "kOk";
    case StatusCodes::kError:
      return "kError";
    case StatusCodes::kOutOfOrderApiCall:
      return "kOutOfOrderApiCall";
    case StatusCodes::kStatusAlreadyStopped:
      return "kStatusAlreadyStopped";
    case StatusCodes::kTransferAlreadyInProgress:
      return "kTransferAlreadyInProgress";
    case StatusCodes::kNoAvailableConnectionMedium:
      return "kNoAvailableConnectionMedium";
    case StatusCodes::kIrrecoverableHardwareError:
      return "kIrrecoverableHardwareError";
    case StatusCodes::kInvalidArgument:
      return "kInvalidArgument";
  }
  LOG(ERROR) << "Unexpected value for StatusCodes: "
             << static_cast<int>(status_code);
  return kUnknownStatusCodesString;
}

}  // namespace sharing
}  // namespace nearby
