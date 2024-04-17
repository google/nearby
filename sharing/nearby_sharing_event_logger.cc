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

#include "sharing/nearby_sharing_event_logger.h"

#include "internal/analytics/event_logger.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/api/preference_manager.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::api::PreferenceManager;

NearbySharingEventLogger::NearbySharingEventLogger(
    const PreferenceManager& preference_manager,
    nearby::analytics::EventLogger* event_logger)
    : preference_manager_(preference_manager), event_logger_(event_logger) {}

NearbySharingEventLogger::~NearbySharingEventLogger() = default;

void NearbySharingEventLogger::Log(
    const location::nearby::analytics::proto::ConnectionsLog& message) {
  if (event_logger_ == nullptr) {
    return;
  }

  if (!preference_manager_.GetBoolean(
          prefs::kNearbySharingIsAnalyticsEnabledName, false)) {
    return;
  }

  event_logger_->Log(message);
}

void NearbySharingEventLogger::Log(
    const sharing::analytics::proto::SharingLog& message) {
  if (event_logger_ == nullptr) {
    return;
  }

  if (!preference_manager_.GetBoolean(
          prefs::kNearbySharingIsAnalyticsEnabledName, false)) {
    return;
  }

  event_logger_->Log(message);
}

void NearbySharingEventLogger::Log(
    const nearby::proto::fastpair::FastPairLog& message) {
  if (event_logger_ == nullptr) {
    return;
  }

  if (!preference_manager_.GetBoolean(
          prefs::kNearbySharingIsAnalyticsEnabledName, false)) {
    return;
  }

  event_logger_->Log(message);
}

void NearbySharingEventLogger::ConfigureExperiments(
    const experiments::ExperimentsLog& message) {
  if (event_logger_ == nullptr) {
    return;
  }

  if (!preference_manager_.GetBoolean(
          prefs::kNearbySharingIsAnalyticsEnabledName, false)) {
    return;
  }

  event_logger_->ConfigureExperiments(message);
}

}  // namespace sharing
}  // namespace nearby
