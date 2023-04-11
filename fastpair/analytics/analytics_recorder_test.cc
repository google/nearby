// Copyright 2023 Google LLC
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

#include "fastpair/analytics/analytics_recorder.h"

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/analytics/event_logger.h"
#include "internal/proto/analytics/fast_pair_log.proto.h"
#include "proto/fast_pair_enums.proto.h"
#include "third_party/protobuf/message_lite.h"

namespace nearby {
namespace fastpair {
namespace analytics {
namespace {

using ::nearby::proto::fastpair::FastPairEvent;
using ::nearby::proto::fastpair::FastPairLog;

class MockEventLogger : public ::nearby::analytics::EventLogger {
 public:
  MockEventLogger() = default;
  ~MockEventLogger() override = default;

  MOCK_METHOD(void, Log, (const ::google::protobuf::MessageLite& message), (override));
};

class AnalyticsRecorderTest : public ::testing::Test {
 public:
  AnalyticsRecorderTest() = default;
  ~AnalyticsRecorderTest() override = default;

  const MockEventLogger& event_logger() { return event_logger_; }

  AnalyticsRecorder analytics_recoder() { return analytics_recorder_; }

 private:
  MockEventLogger event_logger_;
  AnalyticsRecorder analytics_recorder_{&event_logger_};
};

TEST_F(AnalyticsRecorderTest, NewGattEvent) {
  EXPECT_CALL(event_logger(), Log)
      .WillOnce([=](const ::google::protobuf::MessageLite& message) {
        auto log = dynamic_cast<const FastPairLog*>(&message);
        ASSERT_NE(log, nullptr);
        EXPECT_EQ(log->gatt_event().error_from_os(), 1);
      });

  analytics_recoder().NewGattEvent(1);
}

TEST_F(AnalyticsRecorderTest, NewBrEdrHandoverEvent) {
  EXPECT_CALL(event_logger(), Log)
      .WillOnce([=](const ::google::protobuf::MessageLite& message) {
        auto log = dynamic_cast<const FastPairLog*>(&message);
        ASSERT_NE(log, nullptr);
        ASSERT_EQ(log->br_edr_handover_event().error_code(),
                  FastPairEvent::BLUETOOTH_MAC_INVALID);
      });
  analytics_recoder().NewBrEdrHandoverEvent(
      FastPairEvent::BLUETOOTH_MAC_INVALID);
}

TEST_F(AnalyticsRecorderTest, NewCreateBondEvent) {
  EXPECT_CALL(event_logger(), Log)
      .WillOnce([=](const ::google::protobuf::MessageLite& message) {
        auto log = dynamic_cast<const FastPairLog*>(&message);
        ASSERT_NE(log, nullptr);
        ASSERT_EQ(log->bond_event().error_code(), FastPairEvent::NO_PERMISSION);
        ASSERT_EQ(log->bond_event().unbond_reason(), 12);
      });
  analytics_recoder().NewCreateBondEvent(FastPairEvent::NO_PERMISSION, 12);
}

TEST_F(AnalyticsRecorderTest, NewConnectEvent) {
  EXPECT_CALL(event_logger(), Log)
      .WillOnce([=](const ::google::protobuf::MessageLite& message) {
        auto log = dynamic_cast<const FastPairLog*>(&message);
        ASSERT_NE(log, nullptr);
        ASSERT_EQ(log->connect_event().error_code(),
                  FastPairEvent::GET_PROFILE_PROXY_FAILED);
        ASSERT_EQ(log->connect_event().profile_uuid(), 13);
      });
  analytics_recoder().NewConnectEvent(FastPairEvent::GET_PROFILE_PROXY_FAILED,
                                      13);
}

TEST_F(AnalyticsRecorderTest, NewProviderInfo) {
  EXPECT_CALL(event_logger(), Log)
      .WillOnce([=](const ::google::protobuf::MessageLite& message) {
        auto log = dynamic_cast<const FastPairLog*>(&message);
        ASSERT_NE(log, nullptr);
        ASSERT_EQ(log->provider_info().number_account_keys_on_provider(), 14);
      });
  analytics_recoder().NewProviderInfo(14);
}

TEST_F(AnalyticsRecorderTest, NewFootprintsInfo) {
  EXPECT_CALL(event_logger(), Log)
      .WillOnce([=](const ::google::protobuf::MessageLite& message) {
        auto log = dynamic_cast<const FastPairLog*>(&message);
        ASSERT_NE(log, nullptr);
        ASSERT_EQ(log->footprints_info().number_devices_on_footprints(), 15);
      });
  analytics_recoder().NewFootprintsInfo(15);
}

TEST_F(AnalyticsRecorderTest, NewKeyBasedPairingInfo) {
  EXPECT_CALL(event_logger(), Log)
      .WillOnce([=](const ::google::protobuf::MessageLite& message) {
        auto log = dynamic_cast<const FastPairLog*>(&message);
        ASSERT_NE(log, nullptr);
        ASSERT_EQ(log->key_based_pairing_info().request_flag(), 16);
        ASSERT_EQ(log->key_based_pairing_info().response_type(), 17);
        ASSERT_EQ(log->key_based_pairing_info().response_flag(), 18);
        ASSERT_EQ(log->key_based_pairing_info().response_device_count(), 19);
      });
  analytics_recoder().NewKeyBasedPairingInfo(16, 17, 18, 19);
}

}  // namespace
}  // namespace analytics
}  // namespace fastpair
}  // namespace nearby
