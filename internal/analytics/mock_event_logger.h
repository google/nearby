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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_ANALYTICS_MOCK_EVENT_LOGGER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_ANALYTICS_MOCK_EVENT_LOGGER_H_

#include "gmock/gmock.h"
#include "internal/analytics/event_logger.h"

namespace nearby::analytics {

class MockEventLogger : public ::nearby::analytics::EventLogger {
 public:
  MockEventLogger() = default;
  ~MockEventLogger() override = default;

  MOCK_METHOD(
      void, Log,
      (const location::nearby::analytics::proto::ConnectionsLog& message),
      (override));
  MOCK_METHOD(void, Log, (const sharing::analytics::proto::SharingLog& message),
              (override));
  MOCK_METHOD(void, Log, (const nearby::proto::fastpair::FastPairLog& message),
              (override));
};

}  // namespace nearby::analytics

#endif  // THIRD_PARTY_NEARBY_INTERNAL_ANALYTICS_MOCK_EVENT_LOGGER_H_
