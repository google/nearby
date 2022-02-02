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

#ifndef CORE_EVENT_LOGGER_H_
#define CORE_EVENT_LOGGER_H_

#include "internal/proto/analytics/connections_log.pb.h"

namespace location {
namespace nearby {
namespace analytics {

// Allows callers to log |ConnectionsLog| collected at Nearby Connections
// library. Callers need to implement the API if they want to collect this log.
class EventLogger {
 public:
  virtual ~EventLogger() = default;

  // Logs |ConnectionsLog| details. Might block to do I/O, e.g. upload
  // synchronously to some metrics server.
  virtual void Log(const proto::ConnectionsLog& connections_log) = 0;
};

}  // namespace analytics
}  // namespace nearby
}  // namespace location

#endif  // CORE_EVENT_LOGGER_H_
