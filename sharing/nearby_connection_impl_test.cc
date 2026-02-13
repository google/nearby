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

#include "sharing/nearby_connection_impl.h"

#include <memory>
#include <optional>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby::sharing {
namespace {

TEST(NearbyConnectionImpl, DestructorBeforeReaderDestructor) {
  FakeClock fake_clock;
  FakeTaskRunner fake_task_runner(&fake_clock, 1);
  FakeDeviceInfo device_info;
  bool called = false;

  auto connection = std::make_unique<NearbyConnectionImpl>(device_info);
  auto frames_reader = std::make_shared<IncomingFramesReader>(fake_task_runner,
                                                              connection.get());

  absl::Notification notification;
  frames_reader->ReadFrame(
      [&](bool is_timeout,
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        called = true;
        notification.Notify();
      },
      absl::ZeroDuration());
  EXPECT_TRUE(fake_task_runner.SyncWithTimeout(absl::Seconds(1)));
  connection.reset();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(called);
}

TEST(NearbyConnectionImpl, DestructorAfterReaderDestructor) {
  FakeClock fake_clock;
  FakeTaskRunner fake_task_runner(&fake_clock, 1);
  FakeDeviceInfo device_info;
  std::optional<nearby::sharing::service::proto::V1Frame> frame_result;

  auto connection = std::make_unique<NearbyConnectionImpl>(device_info);
  auto frames_reader = std::make_shared<IncomingFramesReader>(fake_task_runner,
                                                              connection.get());

  absl::Notification notification;
  frames_reader->ReadFrame(
      [&](bool is_timeout,
          std::optional<nearby::sharing::service::proto::V1Frame> frame) {
        frame_result = frame;
        notification.Notify();
      },
      absl::ZeroDuration());

  EXPECT_TRUE(fake_task_runner.SyncWithTimeout(absl::Seconds(1)));
  frames_reader.reset();
  connection.reset();
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_FALSE(frame_result.has_value());
}

}  // namespace
}  // namespace nearby::sharing
