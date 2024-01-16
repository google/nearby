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

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/analytics/event_logger.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/proto/analytics/nearby_sharing_log.pb.h"
#include "google/protobuf/message_lite.h"

namespace nearby {
namespace sharing {
namespace {
using ::location::nearby::proto::sharing::EventCategory;
using ::location::nearby::proto::sharing::EventType;
using ::nearby::analytics::EventLogger;
using ::nearby::sharing::analytics::proto::SharingLog;

class MockEventLogger : public EventLogger {
 public:
  ~MockEventLogger() override = default;

  MOCK_METHOD(void, Log, (const proto2::MessageLite& message), (override));
};

class NearbySharingEventLoggerTest : public ::testing::Test {
 public:
  NearbySharingEventLoggerTest() = default;

  void SetUp() override {
    auto event_logger = std::make_unique<MockEventLogger>();
    raw_event_logger_ = event_logger.get();
    sharing_event_logger_ = std::make_unique<NearbySharingEventLogger>(
        preference_manager_, std::move(event_logger));
  }

  void SetEventLogger(bool enabled) {
    preference_manager_.SetBoolean(prefs::kNearbySharingIsAnalyticsEnabledName,
                                   enabled);
  }

  const MockEventLogger* event_logger() { return raw_event_logger_; }

  std::unique_ptr<SharingLog> GetTestEvent() {
    auto sharing_log =
        std::unique_ptr<SharingLog>(SharingLog::default_instance().New());
    sharing_log->set_event_category(EventCategory::SETTINGS_EVENT);
    sharing_log->set_event_type(EventType::TAP_HELP);

    auto tap_help =
        analytics::proto::SharingLog::TapHelp::default_instance().New();

    sharing_log->set_allocated_tap_help(tap_help);
    return sharing_log;
  }

  NearbySharingEventLogger* sharing_event_logger() {
    return sharing_event_logger_.get();
  }

 private:
  nearby::FakePreferenceManager preference_manager_;
  MockEventLogger* raw_event_logger_ = nullptr;
  std::unique_ptr<NearbySharingEventLogger> sharing_event_logger_;
};

TEST_F(NearbySharingEventLoggerTest, LogEventWhenEnabled) {
  SetEventLogger(true);
  EXPECT_CALL(*event_logger(), Log)
      .WillOnce([&](const ::google::protobuf::MessageLite& message) {
        const SharingLog* sharing_log =
            dynamic_cast<const SharingLog*>(&message);
        ASSERT_NE(sharing_log, nullptr);
        EXPECT_EQ(sharing_log->event_category(), EventCategory::SETTINGS_EVENT);
        EXPECT_EQ(sharing_log->event_type(), EventType::TAP_HELP);
      });

  std::unique_ptr<SharingLog> event = GetTestEvent();
  sharing_event_logger()->Log(*event);
}

TEST_F(NearbySharingEventLoggerTest, NoLogEventWhenDisabled) {
  SetEventLogger(false);
  EXPECT_CALL(*event_logger(), Log).Times(0);

  std::unique_ptr<SharingLog> event = GetTestEvent();
  sharing_event_logger()->Log(*event);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
