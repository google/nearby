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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_SERVICE_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_SERVICE_CONTROLLER_H_

#include <memory>

#include "gmock/gmock.h"
#include "presence/implementation/service_controller.h"

namespace nearby {
namespace presence {

/*
 * This class is for unit test, mocking {@code ServiceController} functions.
 */
class MockServiceController : public ServiceController {
 public:
  MockServiceController() = default;
  ~MockServiceController() override = default;

  MOCK_METHOD(absl::StatusOr<ScanSessionId>, StartScan,
              (ScanRequest scan_request, ScanCallback callback), (override));
  MOCK_METHOD(absl::StatusOr<BroadcastSessionId>, StartBroadcast,
              (BroadcastRequest broadcast_request, BroadcastCallback callback),
              (override));

 private:
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_SERVICE_CONTROLLER_H_
