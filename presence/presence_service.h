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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_

#include <memory>

#include "internal/platform/borrowable.h"
#include "presence/data_types.h"
#include "presence/implementation/service_controller.h"
#include "presence/presence_client.h"

namespace nearby {
namespace presence {

/*
 * PresenceService hosts presence functions by routing invokes to the unique
 * {@code ServiceController}. PresenceService should be initialized once and
 * only once in the process that hosting presence functions.
 */
class PresenceService {
 public:
  PresenceService();
  ~PresenceService() { lender_.Release(); }

  PresenceClient CreatePresenceClient();

  absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback);
  void StopScan(ScanSessionId session_id);

  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback);

  void StopBroadcast(BroadcastSessionId session_id);

 private:
  std::unique_ptr<ServiceController> service_controller_;
  ::nearby::Lender<PresenceService *> lender_{this};
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_
