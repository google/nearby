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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_EVENT_LOGGER_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_EVENT_LOGGER_H_

#include "internal/analytics/event_logger.h"
#include "sharing/internal/api/preference_manager.h"

namespace nearby {
namespace sharing {

// Nearby Sharing SDK needs to enable/disable of the event logger according to
// user settings. NearbySharingEventLogger skips analytics logging if analytics
// logging is disabled.
class NearbySharingEventLogger : public nearby::analytics::EventLogger {
 public:
  NearbySharingEventLogger(
      const nearby::sharing::api::PreferenceManager& preference_manager,
      nearby::analytics::EventLogger* event_logger);
  ~NearbySharingEventLogger() override;

  void Log(const location::nearby::analytics::proto::ConnectionsLog& message)
      override;
  void Log(const sharing::analytics::proto::SharingLog& message) override;
  void Log(const nearby::proto::fastpair::FastPairLog& message) override;
  void ConfigureExperiments(
      const experiments::ExperimentsLog& message) override;

 private:
  const nearby::sharing::api::PreferenceManager& preference_manager_;
  nearby::analytics::EventLogger* const event_logger_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_EVENT_LOGGER_H_
