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

#ifndef NEARBY_ANALYTICS_EVENT_LOGGER_H_
#define NEARBY_ANALYTICS_EVENT_LOGGER_H_

#include "internal/proto/analytics/connections_log.pb.h"
#include "internal/proto/analytics/fast_pair_log.pb.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"

namespace nearby {
namespace analytics {

// Allows callers to log the proto collected at the client (e.g. Nearby
// Connections, Nearby Sharing, etc). Callers need to implement the API
// if they want to collect this log.
class EventLogger {
 public:
  virtual ~EventLogger() = default;

  // Logs the proto details. Might block to do I/O, e.g. upload
  // synchronously to some metrics server.
  virtual void Log(
      const location::nearby::analytics::proto::ConnectionsLog& message) = 0;
  virtual void Log(const sharing::analytics::proto::SharingLog& message) = 0;
  virtual void Log(const nearby::proto::fastpair::FastPairLog& message) = 0;
};

}  // namespace analytics
}  // namespace nearby

#endif  // NEARBY_ANALYTICS_EVENT_LOGGER_H_
