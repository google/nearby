// Copyright 2022-2023 Google LLC
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

#include "sharing/incoming_frames_reader.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection_impl.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::service::proto::ConnectionResponseFrame;
using ::nearby::sharing::service::proto::V1Frame;

constexpr absl::Duration kTimeout = absl::Seconds(1);

std::optional<std::vector<uint8_t>> GetIntroductionFrame() {
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::INTRODUCTION);
  v1frame->mutable_introduction();

  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  if (frame.SerializeToArray(data.data(), data.size())) {
    return data;
  }

  return std::nullopt;
}

std::optional<std::vector<uint8_t>> GetCancelFrame() {
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::CANCEL);

  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  if (frame.SerializeToArray(data.data(), data.size())) {
    return data;
  }

  return std::nullopt;
}

std::optional<std::vector<uint8_t>> GetResponseFrame() {
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  frame.set_version(nearby::sharing::service::proto::Frame::V1);
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::RESPONSE);
  v1frame->mutable_connection_response()->set_status(
      ConnectionResponseFrame::ACCEPT);

  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  if (frame.SerializeToArray(data.data(), data.size())) {
    return data;
  }

  return std::nullopt;
}

std::optional<std::vector<uint8_t>> GetInvalidFrame() {
  std::vector<uint8_t> data;
  data.push_back(0xff);
  data.push_back(0x00);
  data.push_back(0x02);
  return data;
}

class IncomingFramesReaderTest : public testing::Test {
 public:
  IncomingFramesReaderTest() {
    nearby_connection_ = std::make_unique<NearbyConnectionImpl>(
        fake_device_info_, &fake_nearby_connections_manager_, "endpoint_id");
  }
  ~IncomingFramesReaderTest() override = default;

  void SetUp() override {
    FakeTaskRunner::ResetPendingTasksCount();
    frames_reader_ = std::make_shared<IncomingFramesReader>(
        fake_task_runner_, nearby_connection_.get());
  }

  NearbyConnectionImpl& connection() {
    NL_CHECK(nearby_connection_);
    return *nearby_connection_;
  }

  IncomingFramesReader* frames_reader() { return frames_reader_.get(); }

  void FastForward(absl::Duration delta) {
    fake_clock_.FastForward(delta);
  }

  void Sync() {
    EXPECT_TRUE(fake_task_runner_.SyncWithTimeout(kTimeout));
  }

  void ReleaseFrameReader() { frames_reader_.reset(); }
  void CloseConnection() {
    nearby_connection_ = nullptr;
  }

 private:
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_ {&fake_clock_, 1};
  FakeDeviceInfo fake_device_info_;
  FakeNearbyConnectionsManager fake_nearby_connections_manager_;
  std::unique_ptr<NearbyConnectionImpl> nearby_connection_;
  std::shared_ptr<IncomingFramesReader> frames_reader_ = nullptr;
};

TEST_F(IncomingFramesReaderTest, ReadTimedOut) {
  absl::Notification notification;
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) {
        EXPECT_EQ(frame, std::nullopt);
        notification.Notify();
      },
      kTimeout);
  Sync();
  FastForward(kTimeout);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReadNonV1FrameSkipped) {
  nearby::sharing::service::proto::Frame frame =
      nearby::sharing::service::proto::Frame();
  V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(service::proto::V1Frame::CANCEL);
  std::vector<uint8_t> data;
  data.resize(frame.ByteSizeLong());
  ASSERT_GT(data.size(), 0);
  ASSERT_TRUE(frame.SerializeToArray(data.data(), data.size()));
  connection().WriteMessage(data);
  std::optional<std::vector<uint8_t>> introduction_frame =
      GetIntroductionFrame();
  ASSERT_TRUE(introduction_frame.has_value());
  connection().WriteMessage(*introduction_frame);

  absl::Notification notification;
  frames_reader()->ReadFrame([&](std::optional<V1Frame> frame) {
    EXPECT_EQ(frame->type(), service::proto::V1Frame::INTRODUCTION);
    notification.Notify();
  });
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReadAnyFrameSuccessful) {
  std::optional<std::vector<uint8_t>> introduction_frame =
      GetIntroductionFrame();
  ASSERT_TRUE(introduction_frame.has_value());
  connection().WriteMessage(*introduction_frame);

  absl::Notification notification;
  frames_reader()->ReadFrame([&](std::optional<V1Frame> frame) {
    EXPECT_EQ(frame->type(), service::proto::V1Frame::INTRODUCTION);
    notification.Notify();
  });
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReadSuccessful) {
  std::optional<std::vector<uint8_t>> introduction_frame =
      GetIntroductionFrame();
  ASSERT_TRUE(introduction_frame.has_value());
  connection().WriteMessage(*introduction_frame);

  absl::Notification notification;
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) {
        EXPECT_EQ(frame->type(), service::proto::V1Frame::INTRODUCTION);
        notification.Notify();
      },
      kTimeout);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReadSuccessful_JumbledFramesOrdering) {
  std::optional<std::vector<uint8_t>> cancel_frame = GetCancelFrame();
  ASSERT_TRUE(cancel_frame.has_value());
  connection().WriteMessage(*cancel_frame);

  std::optional<std::vector<uint8_t>> introduction_frame =
      GetIntroductionFrame();
  ASSERT_TRUE(introduction_frame.has_value());
  connection().WriteMessage(*introduction_frame);

  absl::Notification notification;
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) {
        EXPECT_EQ(frame->type(), service::proto::V1Frame::INTRODUCTION);
        notification.Notify();
      },
      kTimeout);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, JumbledFramesOrdering_ReadFromCache) {
  std::optional<std::vector<uint8_t>> cancel_frame = GetCancelFrame();
  ASSERT_TRUE(cancel_frame.has_value());
  connection().WriteMessage(*cancel_frame);
  std::optional<std::vector<uint8_t>> response_frame = GetResponseFrame();
  ASSERT_TRUE(response_frame.has_value());
  connection().WriteMessage(*response_frame);

  std::optional<std::vector<uint8_t>> introduction_frame =
      GetIntroductionFrame();
  ASSERT_TRUE(introduction_frame.has_value());
  connection().WriteMessage(*introduction_frame);

  absl::Notification notification;
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) {
        EXPECT_EQ(frame->type(), service::proto::V1Frame::INTRODUCTION);
        notification.Notify();
      },
      kTimeout);

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
  // Reading any frame should return cancel frame, then response frame.
  absl::Notification cancel_notification;
  frames_reader()->ReadFrame([&](std::optional<V1Frame> frame) {
    ASSERT_NE(frame, std::nullopt);
    EXPECT_EQ(frame->type(), service::proto::V1Frame::CANCEL);
    cancel_notification.Notify();
  });
  EXPECT_TRUE(cancel_notification.WaitForNotificationWithTimeout(kTimeout));
  absl::Notification response_notification;
  frames_reader()->ReadFrame([&](std::optional<V1Frame> frame) {
    ASSERT_NE(frame, std::nullopt);
    EXPECT_EQ(frame->type(), service::proto::V1Frame::RESPONSE);
    response_notification.Notify();
  });
  EXPECT_TRUE(response_notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReadAfterConnectionClosed) {
  absl::Notification notification;
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) {
        EXPECT_EQ(frame, std::nullopt);
        notification.Notify();
      },
      kTimeout);
  Sync();
  CloseConnection();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReadTwoFramesWithTimeoutSuccessfully) {
  absl::Notification notification;
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) {
        EXPECT_EQ(frame->type(), service::proto::V1Frame::INTRODUCTION);
      },
      kTimeout);
  frames_reader()->ReadFrame(
      service::proto::V1Frame::CANCEL,
      [&](std::optional<V1Frame> frame) {
        EXPECT_EQ(frame->type(), service::proto::V1Frame::CANCEL);
        notification.Notify();
      },
      kTimeout);

  std::optional<std::vector<uint8_t>> cancel_frame = GetCancelFrame();
  ASSERT_TRUE(cancel_frame.has_value());
  connection().WriteMessage(*cancel_frame);

  std::optional<std::vector<uint8_t>> introduction_frame =
      GetIntroductionFrame();
  ASSERT_TRUE(introduction_frame.has_value());
  connection().WriteMessage(*introduction_frame);

  Sync();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReadTwoFramesWithoutTimeoutSuccessfully) {
  absl::Notification notification;
  frames_reader()->ReadFrame([&](std::optional<V1Frame> frame) {
    EXPECT_EQ(frame->type(), service::proto::V1Frame::INTRODUCTION);
  });
  frames_reader()->ReadFrame([&](std::optional<V1Frame> frame) {
    EXPECT_EQ(frame->type(), service::proto::V1Frame::CANCEL);
    notification.Notify();
  });

  std::optional<std::vector<uint8_t>> introduction_frame =
      GetIntroductionFrame();
  ASSERT_TRUE(introduction_frame.has_value());
  connection().WriteMessage(*introduction_frame);

  std::optional<std::vector<uint8_t>> cancel_frame = GetCancelFrame();
  ASSERT_TRUE(cancel_frame.has_value());
  connection().WriteMessage(*cancel_frame);

  Sync();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
}

TEST_F(IncomingFramesReaderTest, ReleaseFrameReaderDuringRead) {
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) { EXPECT_EQ(frame, std::nullopt); },
      kTimeout);
  frames_reader()->ReadFrame(
      service::proto::V1Frame::INTRODUCTION,
      [&](std::optional<V1Frame> frame) { EXPECT_EQ(frame, std::nullopt); },
      kTimeout);
  ReleaseFrameReader();
  EXPECT_EQ(frames_reader(), nullptr);
}

TEST_F(IncomingFramesReaderTest, SkipInvalidFrame) {
  absl::Notification notification;
  frames_reader()->ReadFrame([&](std::optional<V1Frame> frame) {
    EXPECT_EQ(frame->type(), service::proto::V1Frame::CANCEL);
    notification.Notify();
  });

  std::optional<std::vector<uint8_t>> invalid_frame = GetInvalidFrame();
  ASSERT_TRUE(invalid_frame.has_value());
  connection().WriteMessage(*invalid_frame);
  std::optional<std::vector<uint8_t>> cancel_frame = GetCancelFrame();
  ASSERT_TRUE(cancel_frame.has_value());
  connection().WriteMessage(*cancel_frame);

  Sync();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTimeout));
  ReleaseFrameReader();
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
