// Copyright 2020 Google LLC
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

#include "connections/implementation/mediums/bluetooth_classic.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/system_clock.h"

namespace nearby {
namespace connections {
namespace {

using FeatureFlags = FeatureFlags::Flags;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);

class BluetoothClassicTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveryCallback = BluetoothClassicMedium::DiscoveryCallback;

  BluetoothClassicTest() {
    env_.Start();
    radio_a_ = std::make_unique<BluetoothRadio>();
    radio_b_ = std::make_unique<BluetoothRadio>();
    bt_a_ = std::make_unique<BluetoothClassic>(*radio_a_);
    bt_b_ = std::make_unique<BluetoothClassic>(*radio_b_);
    radio_a_->GetBluetoothAdapter().SetName("Device-A");
    radio_b_->GetBluetoothAdapter().SetName("Device-B");
    radio_a_->Enable();
    radio_b_->Enable();
    env_.Sync();
  }

  ~BluetoothClassicTest() override {
    env_.Sync(false);
    radio_a_->Disable();
    radio_b_->Disable();
    bt_a_.reset();
    bt_b_.reset();
    env_.Sync(false);
    radio_a_.reset();
    radio_b_.reset();
    env_.Stop();
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};

  std::unique_ptr<BluetoothRadio> radio_a_;
  std::unique_ptr<BluetoothRadio> radio_b_;
  std::unique_ptr<BluetoothClassic> bt_a_;
  std::unique_ptr<BluetoothClassic> bt_b_;
};

TEST_P(BluetoothClassicTest, CanConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  constexpr absl::string_view kDeviceName{"Simulated BT device #1"};
  constexpr absl::string_view kServiceName{"service name"};

  BluetoothRadio& radio_for_client = *radio_a_;
  BluetoothRadio& radio_for_server = *radio_b_;
  BluetoothClassic& bt_client = *bt_a_;
  BluetoothClassic& bt_server = *bt_b_;

  EXPECT_TRUE(radio_for_client.IsEnabled());
  EXPECT_TRUE(radio_for_server.IsEnabled());

  EXPECT_TRUE(bt_server.TurnOnDiscoverability(std::string(kDeviceName)));
  EXPECT_EQ(radio_for_server.GetBluetoothAdapter().GetName(),
            std::string(kDeviceName));
  CountDownLatch latch(1);
  BluetoothDevice discovered_device;
  EXPECT_TRUE(bt_client.StartDiscovery({
      .device_discovered_cb =
          [&latch, &discovered_device](BluetoothDevice& device) {
            discovered_device = device;
            NEARBY_LOG(INFO, "Discovered device=%p [impl=%p]", &device,
                       &device.GetImpl());
            latch.CountDown();
          },
  }));
  EXPECT_TRUE(latch.Await(kWaitDuration).result());
  EXPECT_TRUE(bt_server.TurnOffDiscoverability());
  ASSERT_TRUE(discovered_device.IsValid());
  BluetoothSocket socket_for_server;
  CountDownLatch accept_latch(1);
  EXPECT_TRUE(bt_server.StartAcceptingConnections(
      std::string(kServiceName),
      {
          .accepted_cb =
              [&socket_for_server, &accept_latch](const std::string& service_id,
                                                  BluetoothSocket socket) {
                socket_for_server = std::move(socket);
                accept_latch.CountDown();
              },
      }));
  CancellationFlag flag;
  BluetoothSocket socket_for_client =
      bt_client.Connect(discovered_device, std::string(kServiceName), &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(bt_server.StopAcceptingConnections(std::string(kServiceName)));
  EXPECT_TRUE(socket_for_server.IsValid());
  EXPECT_TRUE(socket_for_client.IsValid());
  EXPECT_TRUE(socket_for_server.GetRemoteDevice().IsValid());
  EXPECT_TRUE(socket_for_client.GetRemoteDevice().IsValid());
}

TEST_P(BluetoothClassicTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

  constexpr absl::string_view kDeviceName{"Simulated BT device #1"};
  constexpr absl::string_view kServiceName{"service name"};

  BluetoothRadio& radio_for_client = *radio_a_;
  BluetoothRadio& radio_for_server = *radio_b_;
  BluetoothClassic& bt_client = *bt_a_;
  BluetoothClassic& bt_server = *bt_b_;

  EXPECT_TRUE(radio_for_client.IsEnabled());
  EXPECT_TRUE(radio_for_server.IsEnabled());

  EXPECT_TRUE(bt_server.TurnOnDiscoverability(std::string(kDeviceName)));
  EXPECT_EQ(radio_for_server.GetBluetoothAdapter().GetName(),
            std::string(kDeviceName));
  CountDownLatch latch(1);
  BluetoothDevice discovered_device;
  EXPECT_TRUE(bt_client.StartDiscovery({
      .device_discovered_cb =
          [&latch, &discovered_device](BluetoothDevice& device) {
            discovered_device = device;
            NEARBY_LOG(INFO, "Discovered device=%p [impl=%p]", &device,
                       &device.GetImpl());
            latch.CountDown();
          },
  }));
  EXPECT_TRUE(latch.Await(kWaitDuration).result());
  EXPECT_TRUE(bt_server.TurnOffDiscoverability());
  ASSERT_TRUE(discovered_device.IsValid());
  BluetoothSocket socket_for_server;
  CountDownLatch accept_latch(1);
  EXPECT_TRUE(bt_server.StartAcceptingConnections(
      std::string(kServiceName),
      {
          .accepted_cb =
              [&socket_for_server, &accept_latch](const std::string& service_id,
                                                  BluetoothSocket socket) {
                socket_for_server = std::move(socket);
                accept_latch.CountDown();
              },
      }));
  CancellationFlag flag(true);
  BluetoothSocket socket_for_client =
      bt_client.Connect(discovered_device, std::string(kServiceName), &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(bt_server.StopAcceptingConnections(std::string(kServiceName)));
    EXPECT_TRUE(socket_for_server.IsValid());
    EXPECT_TRUE(socket_for_client.IsValid());
    EXPECT_TRUE(socket_for_server.GetRemoteDevice().IsValid());
    EXPECT_TRUE(socket_for_client.GetRemoteDevice().IsValid());
  } else {
    EXPECT_FALSE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(bt_server.StopAcceptingConnections(std::string(kServiceName)));
    EXPECT_FALSE(socket_for_server.IsValid());
    EXPECT_FALSE(socket_for_client.IsValid());
  }
}

INSTANTIATE_TEST_SUITE_P(ParametrisedBluetoothClassicTest, BluetoothClassicTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(BluetoothClassicTest, CanConstructValidObject) {
  EXPECT_TRUE(bt_a_->IsMediumValid());
  EXPECT_TRUE(bt_a_->IsAdapterValid());
  EXPECT_TRUE(bt_a_->IsAvailable());
  EXPECT_TRUE(bt_b_->IsMediumValid());
  EXPECT_TRUE(bt_b_->IsAdapterValid());
  EXPECT_TRUE(bt_b_->IsAvailable());
  EXPECT_NE(&radio_a_->GetBluetoothAdapter(), &radio_b_->GetBluetoothAdapter());
}

TEST_F(BluetoothClassicTest, CanStartAdvertising) {
  constexpr absl::string_view kDeviceName{"Simulated BT device #1"};
  EXPECT_TRUE(bt_a_->TurnOnDiscoverability(std::string(kDeviceName)));
  EXPECT_EQ(radio_a_->GetBluetoothAdapter().GetName(), kDeviceName);
}

TEST_F(BluetoothClassicTest, CanStopAdvertising) {
  constexpr absl::string_view kDeviceName{"Simulated BT device #1"};
  EXPECT_TRUE(bt_a_->TurnOnDiscoverability(std::string(kDeviceName)));
  EXPECT_EQ(radio_a_->GetBluetoothAdapter().GetName(), kDeviceName);
  EXPECT_TRUE(bt_a_->TurnOffDiscoverability());
}

TEST_F(BluetoothClassicTest, CanStartDiscovery) {
  constexpr absl::string_view kDeviceName{"Simulated BT device #1"};
  EXPECT_TRUE(bt_a_->TurnOnDiscoverability(std::string(kDeviceName)));
  EXPECT_EQ(radio_a_->GetBluetoothAdapter().GetName(), kDeviceName);
  CountDownLatch latch(1);
  EXPECT_TRUE(bt_b_->StartDiscovery({
      .device_discovered_cb =
          [&latch](BluetoothDevice& device) { latch.CountDown(); },
  }));
  EXPECT_TRUE(latch.Await(kWaitDuration).result());
  EXPECT_TRUE(bt_a_->TurnOffDiscoverability());
}

TEST_F(BluetoothClassicTest, CanStopDiscovery) {
  CountDownLatch latch(1);
  EXPECT_TRUE(bt_a_->StartDiscovery({
      .device_discovered_cb =
          [&latch](BluetoothDevice& device) { latch.CountDown(); },
  }));
  EXPECT_FALSE(latch.Await(kWaitDuration).result());
  EXPECT_TRUE(bt_a_->StopDiscovery());
}

TEST_F(BluetoothClassicTest, CanStartAcceptingConnections) {
  constexpr absl::string_view kDeviceName{"Simulated BT device #1"};
  constexpr absl::string_view kServiceName{"service name"};

  BluetoothRadio& radio_for_client = *radio_a_;
  BluetoothRadio& radio_for_server = *radio_b_;
  BluetoothClassic& bt_client = *bt_a_;
  BluetoothClassic& bt_server = *bt_b_;

  EXPECT_TRUE(radio_for_client.IsEnabled());
  EXPECT_TRUE(radio_for_server.IsEnabled());

  EXPECT_TRUE(bt_server.TurnOnDiscoverability(std::string(kDeviceName)));
  EXPECT_EQ(radio_for_server.GetBluetoothAdapter().GetName(), kDeviceName);
  CountDownLatch latch(1);
  BluetoothDevice discovered_device;
  EXPECT_TRUE(bt_client.StartDiscovery({
      .device_discovered_cb =
          [&latch, &discovered_device](BluetoothDevice& device) {
            discovered_device = device;
            NEARBY_LOG(INFO, "Discovered device=%p [impl=%p]", &device,
                       &device.GetImpl());
            latch.CountDown();
          },
  }));
  EXPECT_TRUE(latch.Await(kWaitDuration).result());
  EXPECT_TRUE(bt_server.TurnOffDiscoverability());
  EXPECT_TRUE(discovered_device.IsValid());
  EXPECT_TRUE(
      bt_server.StartAcceptingConnections(std::string(kServiceName), {}));
  // Allow StartAcceptingConnections do something, before stopping it.
  // This is best effort, because no callbacks are invoked in this scenario.
  SystemClock::Sleep(kWaitDuration);
  EXPECT_TRUE(bt_server.StopAcceptingConnections(std::string(kServiceName)));
}

}  // namespace
}  // namespace connections
}  // namespace nearby
