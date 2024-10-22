// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_ANALYTICS_SHARING_LOG_MATCHERS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_ANALYTICS_SHARING_LOG_MATCHERS_H_

#include "gmock/gmock.h"

namespace nearby::analytics {

MATCHER_P(HasCategory, category, "has category") {
  return arg.event_category() == category;
}

MATCHER_P(HasEventType, event_type, "has event type") {
  return arg.event_type() == event_type;
}

MATCHER_P(HasAction, action, "has action") {
  return arg.action() == action;
}

MATCHER_P(HasSessionId, session_id, "has session id") {
  return arg.session_id() == session_id;
}

MATCHER_P(HasDurationMillis, duration_millis, "has duration millis") {
  return arg.duration_millis() == duration_millis;
}

MATCHER_P(SharingLogHasStatus, status, "has status") {
  return arg.status() == status;
}

MATCHER_P(HasRpcName, rpc_name, "has rpc_name") {
  return arg.rpc_name() == rpc_name;
}

MATCHER_P(HasDirection, direction, "has direction") {
  return arg.direction() == direction;
}

MATCHER_P(HasErrorCode, error_code, "has error_code") {
  return arg.error_code() == error_code;
}

MATCHER_P(HasLatencyMillis, latency_millis, "has latency_millis") {
  return arg.latency_millis() == latency_millis;
}

}  // namespace nearby::analytics

#endif  // THIRD_PARTY_NEARBY_INTERNAL_ANALYTICS_SHARING_LOG_MATCHERS_H_
