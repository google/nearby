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

#include "platform_v2/public/bluetooth_classic.h"

#include <memory>

#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/bluetooth_adapter.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/single_thread_executor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace {

class BluetoothClassicMediumTest : public ::testing::Test {
 protected:
  using DiscoveryCallback = BluetoothClassicMedium::DiscoveryCallback;
  BluetoothClassicMediumTest() {
    env_.Start();
    env_.Reset();
    adapter_a_ = std::make_unique<BluetoothAdapter>();
    adapter_b_ = std::make_unique<BluetoothAdapter>();
    bt_a_ = std::make_unique<BluetoothClassicMedium>(*adapter_a_);
    bt_b_ = std::make_unique<BluetoothClassicMedium>(*adapter_b_);
    adapter_a_->SetName("Device-A");
    adapter_b_->SetName("Device-B");
    adapter_a_->SetStatus(BluetoothAdapter::Status::kEnabled);
    adapter_b_->SetStatus(BluetoothAdapter::Status::kEnabled);
    env_.Sync();
  }
  ~BluetoothClassicMediumTest() override {
    env_.Sync(false);
    adapter_a_->SetStatus(BluetoothAdapter::Status::kDisabled);
    adapter_b_->SetStatus(BluetoothAdapter::Status::kDisabled);
    bt_a_.reset();
    bt_b_.reset();
    env_.Sync(false);
    adapter_a_.reset();
    adapter_b_.reset();
    env_.Reset();
    env_.Stop();
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};

  std::unique_ptr<BluetoothAdapter> adapter_a_;
  std::unique_ptr<BluetoothAdapter> adapter_b_;
  std::unique_ptr<BluetoothClassicMedium> bt_a_;
  std::unique_ptr<BluetoothClassicMedium> bt_b_;
};

TEST_F(BluetoothClassicMediumTest, ConstructorDestructorWorks) {
  // Make sure we can create functional adapters.
  ASSERT_TRUE(adapter_a_->IsValid());
  ASSERT_TRUE(adapter_b_->IsValid());

  // Make sure we can create 2 distinct adapters.
  // NOTE: multiple adapters are supported on a test platform, but not
  // necessarily on every available HW platform.
  // Often, HW platform supports only one BT adapter.
  EXPECT_NE(&adapter_a_->GetImpl(), &adapter_b_->GetImpl());

  // Make sure we can create functional mediums.
  ASSERT_TRUE(bt_a_->IsValid());
  ASSERT_TRUE(bt_b_->IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&bt_a_->GetImpl(), &bt_b_->GetImpl());
}

TEST_F(BluetoothClassicMediumTest, CanStartDiscovery) {
  adapter_a_->SetScanMode(BluetoothAdapter::ScanMode::kConnectable);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  bt_a_->StartDiscovery(DiscoveryCallback{
      .device_discovered_cb =
          [this, &found_latch](BluetoothDevice& device) {
            NEARBY_LOG(INFO, "Device discovered: %s", device.GetName().c_str());
            EXPECT_EQ(device.GetName(), adapter_b_->GetName());
            found_latch.CountDown();
          },
      .device_lost_cb =
          [this, &lost_latch](BluetoothDevice& device) {
            NEARBY_LOG(INFO, "Device lost: %s", device.GetName().c_str());
            EXPECT_EQ(device.GetName(), adapter_b_->GetName());
            lost_latch.CountDown();
          },
  });
  adapter_b_->SetScanMode(BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_EQ(adapter_b_->GetScanMode(),
            BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  adapter_b_->SetStatus(BluetoothAdapter::Status::kDisabled);
  EXPECT_FALSE(adapter_b_->IsEnabled());
  EXPECT_TRUE(lost_latch.Await(absl::Milliseconds(1000)).result());
}

TEST_F(BluetoothClassicMediumTest, CanStopDiscovery) {
  adapter_a_->SetScanMode(BluetoothAdapter::ScanMode::kConnectable);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);
  bt_a_->StartDiscovery(DiscoveryCallback{
      .device_discovered_cb =
          [this, &found_latch](BluetoothDevice& device) {
            NEARBY_LOG(INFO, "Device discovered: %s", device.GetName().c_str());
            EXPECT_EQ(device.GetName(), adapter_b_->GetName());
            found_latch.CountDown();
          },
      .device_lost_cb =
          [this, &lost_latch](BluetoothDevice& device) {
            NEARBY_LOG(INFO, "Device lost: %s", device.GetName().c_str());
            EXPECT_EQ(device.GetName(), adapter_b_->GetName());
            lost_latch.CountDown();
          },
  });
  adapter_b_->SetScanMode(BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_EQ(adapter_b_->GetScanMode(),
            BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  bt_a_->StopDiscovery();
  adapter_b_->SetStatus(BluetoothAdapter::Status::kDisabled);
  EXPECT_FALSE(adapter_b_->IsEnabled());
  EXPECT_FALSE(lost_latch.Await(absl::Milliseconds(1000)).result());
}

TEST_F(BluetoothClassicMediumTest, CanListenForService) {
  adapter_a_->SetScanMode(BluetoothAdapter::ScanMode::kConnectable);
  CountDownLatch found_latch(1);
  bt_a_->StartDiscovery(DiscoveryCallback{
      .device_discovered_cb =
          [this, &found_latch](BluetoothDevice& device) {
            NEARBY_LOG(INFO, "Device discovered: %s", device.GetName().c_str());
            EXPECT_EQ(device.GetName(), adapter_b_->GetName());
            found_latch.CountDown();
          },
  });
  adapter_b_->SetScanMode(BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_EQ(adapter_b_->GetScanMode(),
            BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  std::string service_name{"service"};
  std::string service_uuid("service-uuid");
  BluetoothServerSocket server_socket =
      bt_b_->ListenForService(service_name, service_uuid);
  EXPECT_TRUE(server_socket.IsValid());
  server_socket.Close();
}

TEST_F(BluetoothClassicMediumTest, CanConnectToService) {
  adapter_a_->SetScanMode(BluetoothAdapter::ScanMode::kConnectable);
  CountDownLatch found_latch(1);
  BluetoothDevice* discovered_device = nullptr;
  bt_a_->StartDiscovery(DiscoveryCallback{
      .device_discovered_cb =
          [this, &found_latch, &discovered_device](BluetoothDevice& device) {
            NEARBY_LOG(INFO, "Device discovered: %s", device.GetName().c_str());
            EXPECT_EQ(device.GetName(), adapter_b_->GetName());
            discovered_device = &device;
            found_latch.CountDown();
          },
  });
  adapter_b_->SetScanMode(BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_EQ(adapter_b_->GetScanMode(),
            BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  std::string service_name{"service"};
  std::string service_uuid("service-uuid");
  BluetoothServerSocket server_socket =
      bt_b_->ListenForService(service_name, service_uuid);
  EXPECT_TRUE(server_socket.IsValid());
  BluetoothSocket socket_a;
  BluetoothSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  EXPECT_FALSE(socket_b.IsValid());
  {
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [this, &socket_a, discovered_device, &service_uuid, &server_socket]() {
          socket_a = bt_a_->ConnectToService(*discovered_device, service_uuid);
          if (!socket_a.IsValid()) server_socket.Close();
        });
    server_executor.Execute(
        [&socket_b, &server_socket]() {
          socket_b = server_socket.Accept();
          if (!socket_b.IsValid()) server_socket.Close();
        });
  }
  EXPECT_TRUE(socket_a.IsValid());
  EXPECT_TRUE(socket_b.IsValid());
  server_socket.Close();
}

}  // namespace
}  // namespace nearby
}  // namespace location
