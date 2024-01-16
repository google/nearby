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

#include "sharing/transfer_manager.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {
namespace {

constexpr absl::string_view kEndpointId = "endpoint";
constexpr absl::Duration kNotificationTimeout = absl::Milliseconds(200);

TEST(TransferManager, MediumUpgradeSuccess) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;

  TransferManager transfer_manager{&context, kEndpointId};
  transfer_manager.Send([&]() {
    is_called = true;
    notification.Notify();
  });

  ASSERT_FALSE(is_called);
  ASSERT_TRUE(transfer_manager.StartTransfer());
  ASSERT_FALSE(transfer_manager.StartTransfer());
  transfer_manager.OnMediumQualityChanged(Medium::kWifiLan);
  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(kNotificationTimeout));
  ASSERT_TRUE(is_called);
  ASSERT_FALSE(transfer_manager.StartTransfer());
}

TEST(TransferManager, SendAfterMediumUpgradeSuccess) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;

  TransferManager transfer_manager{&context, kEndpointId};
  transfer_manager.Send([&]() {
    is_called = true;
    notification.Notify();
  });

  ASSERT_FALSE(is_called);
  ASSERT_TRUE(transfer_manager.StartTransfer());
  transfer_manager.OnMediumQualityChanged(Medium::kWebRtc);
  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(kNotificationTimeout));
  ASSERT_TRUE(is_called);
  is_called = false;
  transfer_manager.Send([&]() { is_called = true; });
  ASSERT_TRUE(is_called);
}

TEST(TransferManager, MediumUpgradeFailed) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;

  TransferManager transfer_manager{&context, kEndpointId};
  transfer_manager.Send([&]() {
    is_called = true;
    notification.Notify();
  });

  ASSERT_FALSE(is_called);
  ASSERT_TRUE(transfer_manager.StartTransfer());
  transfer_manager.OnMediumQualityChanged(Medium::kBluetooth);
  ASSERT_FALSE(
      notification.WaitForNotificationWithTimeout(kNotificationTimeout));
  ASSERT_FALSE(is_called);
}

TEST(TransferManager, MediumUpgradeTimeout) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;

  TransferManager transfer_manager{&context, kEndpointId};
  transfer_manager.Send([&]() {
    is_called = true;
    notification.Notify();
  });

  ASSERT_FALSE(is_called);
  ASSERT_TRUE(transfer_manager.StartTransfer());
  FakeClock* clock = static_cast<FakeClock*>(context.GetClock());
  clock->FastForward(TransferManager::kMediumUpgradeTimeout);

  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(kNotificationTimeout));
  ASSERT_TRUE(is_called);
}

TEST(TransferManager, CancelStartedTransfer) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;

  TransferManager transfer_manager{&context, kEndpointId};
  transfer_manager.Send([&]() {
    is_called = true;
    notification.Notify();
  });

  ASSERT_FALSE(is_called);
  ASSERT_TRUE(transfer_manager.StartTransfer());
  FakeClock* clock = static_cast<FakeClock*>(context.GetClock());
  clock->FastForward(absl::Seconds(5));
  ASSERT_TRUE(transfer_manager.CancelTransfer());

  ASSERT_FALSE(
      notification.WaitForNotificationWithTimeout(kNotificationTimeout));
  ASSERT_FALSE(is_called);
}

TEST(TransferManager, CancelTimedOutMediumUpgrade) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;

  TransferManager transfer_manager{&context, kEndpointId};
  transfer_manager.Send([&]() {
    is_called = true;
    notification.Notify();
  });

  ASSERT_FALSE(is_called);
  ASSERT_TRUE(transfer_manager.StartTransfer());
  FakeClock* clock = static_cast<FakeClock*>(context.GetClock());
  clock->FastForward(TransferManager::kMediumUpgradeTimeout);

  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(kNotificationTimeout));
  ASSERT_TRUE(is_called);
  ASSERT_FALSE(transfer_manager.CancelTransfer());
}

TEST(TransferManager, MediumUpgradeBeforeStartTransfer) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;

  TransferManager transfer_manager{&context, kEndpointId};
  transfer_manager.Send([&]() {
    is_called = true;
    notification.Notify();
  });

  transfer_manager.OnMediumQualityChanged(Medium::kWifiLan);

  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(kNotificationTimeout));
  ASSERT_TRUE(is_called);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
