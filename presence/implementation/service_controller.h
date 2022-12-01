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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_H_

#include <memory>

#include "absl/status/statusor.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

/*
 * This class is owned in {@code PresenceService}. It specifies the function
 * signatures. {@code ServiceControllerImpl} and {@code MockServiceController}
 * inherit this class and provides real implementation and mock impl for tests.
 */
class ServiceController {
 public:
  ServiceController() = default;
  virtual ~ServiceController() = default;
  virtual absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                                  ScanCallback callback) = 0;
  virtual void StopScan(ScanSessionId session_id) = 0;
  virtual absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) = 0;
  virtual void StopBroadcast(BroadcastSessionId session_id) = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_H_
