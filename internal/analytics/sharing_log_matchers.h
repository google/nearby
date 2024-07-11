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

}  // namespace nearby::analytics

#endif  // THIRD_PARTY_NEARBY_INTERNAL_ANALYTICS_SHARING_LOG_MATCHERS_H_
