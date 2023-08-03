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

#include "internal/platform/bluetooth_classic.h"

#include <memory>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace {

using FeatureFlags = FeatureFlags::Flags;
using PairingError = api::BluetoothPairingCallback::PairingError;
constexpr absl::string_view kPairingPasskKey("123456");

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

class BluetoothClassicMediumObserver
    : public BluetoothClassicMedium ::Observer {
 public:
  explicit BluetoothClassicMediumObserver(
      CountDownLatch* device_added_latch, CountDownLatch* device_removed_latch,
      CountDownLatch* device_paired_changed_latch)
      : device_added_latch_(device_added_latch),
        device_removed_latch_(device_removed_latch),
        device_paired_changed_latch_(device_paired_changed_latch) {}

  void DeviceAdded(BluetoothDevice& device) override {
    if (!device_added_latch_) return;
    device_added_latch_->CountDown();
  }

  void DeviceRemoved(BluetoothDevice& device) override {
    if (!device_removed_latch_) return;
    device_removed_latch_->CountDown();
  }

  void DevicePairedChanged(BluetoothDevice& device,
                           bool new_paired_status) override {
    if (!device_paired_changed_latch_) return;
    paired_status_ = new_paired_status;
    device_paired_changed_latch_->CountDown();
  }

  CountDownLatch* device_added_latch_;
  CountDownLatch* device_removed_latch_;
  CountDownLatch* device_paired_changed_latch_;
  bool paired_status_ = false;
};

class BluetoothClassicMediumTest
    : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveryCallback = BluetoothClassicMedium::DiscoveryCallback;
  BluetoothClassicMediumTest() {
    env_.Start();
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
    env_.Stop();
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};

  std::unique_ptr<BluetoothAdapter> adapter_a_;
  std::unique_ptr<BluetoothAdapter> adapter_b_;
  std::unique_ptr<BluetoothClassicMedium> bt_a_;
  std::unique_ptr<BluetoothClassicMedium> bt_b_;
};

TEST_P(BluetoothClassicMediumTest, CanConnectToService) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

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
    CancellationFlag flag;
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute([this, &socket_a, discovered_device, &service_uuid,
                             &server_socket, &flag]() {
      socket_a =
          bt_a_->ConnectToService(*discovered_device, service_uuid, &flag);
      if (!socket_a.IsValid()) server_socket.Close();
    });
    server_executor.Execute([&socket_b, &server_socket]() {
      socket_b = server_socket.Accept();
      if (!socket_b.IsValid()) server_socket.Close();
    });
  }
  EXPECT_TRUE(socket_a.IsValid());
  EXPECT_TRUE(socket_b.IsValid());
  server_socket.Close();
}

TEST_P(BluetoothClassicMediumTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);

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
    CancellationFlag flag(true);
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute([this, &socket_a, discovered_device, &service_uuid,
                             &server_socket, &flag]() {
      socket_a =
          bt_a_->ConnectToService(*discovered_device, service_uuid, &flag);
      if (!socket_a.IsValid()) server_socket.Close();
    });
    server_executor.Execute([&socket_b, &server_socket]() {
      socket_b = server_socket.Accept();
      if (!socket_b.IsValid()) server_socket.Close();
    });
  }
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(socket_a.IsValid());
    EXPECT_TRUE(socket_b.IsValid());
  } else {
    EXPECT_FALSE(socket_a.IsValid());
    EXPECT_FALSE(socket_b.IsValid());
  }
  server_socket.Close();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedBluetoothClassicMediumTest,
                         BluetoothClassicMediumTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(BluetoothClassicMediumTest, SendData) {
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
  ASSERT_TRUE(found_latch.Await().Ok());
  std::string service_name{"service"};
  std::string service_uuid("service-uuid");
  BluetoothServerSocket server_socket =
      bt_b_->ListenForService(service_name, service_uuid);
  ASSERT_TRUE(server_socket.IsValid());
  {
    ByteArray data("data");
    CancellationFlag flag;
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute([&, this]() {
      BluetoothSocket socket_a =
          bt_a_->ConnectToService(*discovered_device, service_uuid, &flag);
      ASSERT_TRUE(socket_a.IsValid());
      EXPECT_TRUE(socket_a.GetOutputStream().Write(data).Ok());
      EXPECT_TRUE(socket_a.GetOutputStream().Close().Ok());
    });
    server_executor.Execute([&]() {
      BluetoothSocket socket_b = server_socket.Accept();
      ASSERT_TRUE(socket_b.IsValid());
      ExceptionOr<ByteArray> result =
          socket_b.GetInputStream().Read(data.size());
      ASSERT_EQ(result.exception(), Exception::kSuccess);
      ASSERT_TRUE(result.ok());
      EXPECT_EQ(result.GetResult(), data);
    });
  }
  server_socket.Close();
}

TEST_F(BluetoothClassicMediumTest, IoOnClosedSocketReturnsEmpty) {
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
  ASSERT_TRUE(found_latch.Await().Ok());
  std::string service_name{"service"};
  std::string service_uuid("service-uuid");
  BluetoothServerSocket server_socket =
      bt_b_->ListenForService(service_name, service_uuid);
  ASSERT_TRUE(server_socket.IsValid());
  {
    ByteArray data("data");
    CancellationFlag flag;
    SingleThreadExecutor server_executor;
    SingleThreadExecutor client_executor;
    client_executor.Execute([&, this]() {
      BluetoothSocket socket_a =
          bt_a_->ConnectToService(*discovered_device, service_uuid, &flag);
      ASSERT_TRUE(socket_a.IsValid());
      socket_a.Close();
      EXPECT_FALSE(socket_a.GetOutputStream().Write(data).Ok());
    });
    server_executor.Execute([&]() {
      BluetoothSocket socket_b = server_socket.Accept();
      ASSERT_TRUE(socket_b.IsValid());
      socket_b.Close();
      EXPECT_TRUE(socket_b.GetInputStream().Read(data.size()).result().Empty());
    });
  }
  server_socket.Close();
}

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
  CountDownLatch device_added_latch(1);
  CountDownLatch device_removed_latch(1);
  BluetoothClassicMediumObserver observer(&device_added_latch,
                                          &device_removed_latch, nullptr);
  bt_a_->AddObserver(&observer);

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
  EXPECT_TRUE(device_added_latch.Await(absl::Milliseconds(1000)).result());
  adapter_b_->SetStatus(BluetoothAdapter::Status::kDisabled);
  EXPECT_FALSE(adapter_b_->IsEnabled());
  EXPECT_TRUE(lost_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_TRUE(device_removed_latch.Await(absl::Milliseconds(1000)).result());
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

TEST_F(BluetoothClassicMediumTest, FailIfDiscovering) {
  EXPECT_TRUE(bt_a_->StartDiscovery({}));
  EXPECT_FALSE(bt_a_->StartDiscovery({}));
}

TEST_F(BluetoothClassicMediumTest, BluetoothPairingSuccess) {
  // Discovered remote device
  adapter_a_->SetScanMode(BluetoothAdapter::ScanMode::kConnectable);
  BluetoothDevice* discovered_device = nullptr;
  CountDownLatch found_latch(1);
  bt_a_->StartDiscovery(
      DiscoveryCallback{.device_discovered_cb = [&](BluetoothDevice& device) {
        NEARBY_LOG(INFO, "Device discovered: %s", device.GetName().c_str());
        EXPECT_EQ(device.GetName(), adapter_b_->GetName());
        discovered_device = &device;
        found_latch.CountDown();
      }});
  adapter_b_->SetScanMode(BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  found_latch.Await();

  // Create bluetooth pairing instance to handle the pairing process
  // with the remote device
  auto bluetooth_pairing = bt_a_->CreatePairing(*discovered_device);
  EXPECT_TRUE(bluetooth_pairing);
  EXPECT_FALSE(bluetooth_pairing->IsPaired());

  // Configure the remote device's pairing context
  api::PairingParams pairing_params;
  pairing_params.pairing_type =
      api::PairingParams::PairingType::kConfirmPasskey;
  pairing_params.passkey = kPairingPasskKey;
  env_.ConfigBluetoothPairingContext(&discovered_device->GetImpl(),
                                     pairing_params);

  // Initiates pairing request with remote device
  std::string received_passkey;
  CountDownLatch paired_latch(1);
  CountDownLatch initiated_latch(1);
  CountDownLatch error_latch(1);
  CountDownLatch device_paired_latch(1);
  BluetoothClassicMediumObserver observer(nullptr, nullptr,
                                          &device_paired_latch);
  bt_a_->AddObserver(&observer);
  EXPECT_TRUE(bluetooth_pairing->InitiatePairing({
      .on_paired_cb = [&]() { paired_latch.CountDown(); },
      .on_pairing_error_cb =
          [&](api::BluetoothPairingCallback::PairingError error) {
            EXPECT_EQ(
                error,
                api::BluetoothPairingCallback::PairingError::kAuthTimeout);
            error_latch.CountDown();
          },
      .on_pairing_initiated_cb =
          [&](api::PairingParams pairingParams) {
            EXPECT_EQ(pairingParams.passkey, pairing_params.passkey);
            EXPECT_EQ(pairingParams.pairing_type, pairing_params.pairing_type);
            received_passkey = pairingParams.passkey;
            initiated_latch.CountDown();
          },
  }));
  initiated_latch.Await();

  // Mocks pairing success result for the remote device.
  EXPECT_TRUE(
      env_.SetPairingResult(&discovered_device->GetImpl(), std::nullopt));
  // Finishes pairing with remote device.
  EXPECT_TRUE(bluetooth_pairing->FinishPairing(received_passkey));
  paired_latch.Await();
  device_paired_latch.Await();
  EXPECT_TRUE(observer.paired_status_);
  EXPECT_TRUE(bluetooth_pairing->IsPaired());

  // Unpairs with remote device.
  EXPECT_TRUE(bluetooth_pairing->Unpair());
  EXPECT_FALSE(bluetooth_pairing->IsPaired());
}

TEST_F(BluetoothClassicMediumTest, BluetoothPairingFailure) {
  // Discovered remote device
  adapter_a_->SetScanMode(BluetoothAdapter::ScanMode::kConnectable);
  BluetoothDevice* discovered_device = nullptr;
  CountDownLatch found_latch(1);
  bt_a_->StartDiscovery(
      DiscoveryCallback{.device_discovered_cb = [&](BluetoothDevice& device) {
        NEARBY_LOG(INFO, "Device discovered: %s", device.GetName().c_str());
        EXPECT_EQ(device.GetName(), adapter_b_->GetName());
        discovered_device = &device;
        found_latch.CountDown();
      }});
  adapter_b_->SetScanMode(BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  found_latch.Await();

  // Create bluetooth pairing instance to handle the pairing process
  // with the remote device
  auto bluetooth_pairing = bt_a_->CreatePairing(*discovered_device);
  EXPECT_TRUE(bluetooth_pairing);
  EXPECT_FALSE(bluetooth_pairing->IsPaired());

  // Configure the remote device's pairing context
  api::PairingParams pairing_params;
  pairing_params.pairing_type =
      api::PairingParams::PairingType::kConfirmPasskey;
  pairing_params.passkey = kPairingPasskKey;
  env_.ConfigBluetoothPairingContext(&discovered_device->GetImpl(),
                                     pairing_params);

  // Initiates pairing request with remote device
  std::string received_passkey;
  CountDownLatch paired_latch(1);
  CountDownLatch initiated_latch(1);
  CountDownLatch error_latch(1);
  EXPECT_TRUE(bluetooth_pairing->InitiatePairing({
      .on_paired_cb = [&]() { paired_latch.CountDown(); },
      .on_pairing_error_cb =
          [&](api::BluetoothPairingCallback::PairingError error) {
            EXPECT_EQ(
                error,
                api::BluetoothPairingCallback::PairingError::kAuthTimeout);
            error_latch.CountDown();
          },
      .on_pairing_initiated_cb =
          [&](api::PairingParams pairingParams) {
            EXPECT_EQ(pairingParams.passkey, pairing_params.passkey);
            EXPECT_EQ(pairingParams.pairing_type, pairing_params.pairing_type);
            received_passkey = pairingParams.passkey;
            initiated_latch.CountDown();
          },
  }));
  initiated_latch.Await();

  // Mocks pairing failure result for the remote device.
  EXPECT_TRUE(env_.SetPairingResult(
      &discovered_device->GetImpl(),
      api::BluetoothPairingCallback::PairingError::kAuthTimeout));
  // Finishes pairing with remote device.
  EXPECT_TRUE(bluetooth_pairing->FinishPairing(received_passkey));
  error_latch.Await();
  EXPECT_FALSE(bluetooth_pairing->IsPaired());
}
TEST_F(BluetoothClassicMediumTest, CancelBluetoothPairing) {
  // Discovered remote device
  adapter_a_->SetScanMode(BluetoothAdapter::ScanMode::kConnectable);
  BluetoothDevice* discovered_device = nullptr;
  CountDownLatch found_latch(1);
  bt_a_->StartDiscovery(
      DiscoveryCallback{.device_discovered_cb = [&](BluetoothDevice& device) {
        NEARBY_LOG(INFO, "Device discovered: %s", device.GetName().c_str());
        NEARBY_LOG(INFO, "Device discovered address: %s",
                   device.GetMacAddress().c_str());
        EXPECT_EQ(device.GetName(), adapter_b_->GetName());
        discovered_device = &device;
        found_latch.CountDown();
      }});
  adapter_b_->SetScanMode(BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  found_latch.Await();

  // Create bluetooth pairing instance to handle the pairing process
  // with the remote device
  auto bluetooth_pairing = bt_a_->CreatePairing(*discovered_device);
  EXPECT_FALSE(bluetooth_pairing->IsPaired());

  // Configure the remote device's pairing context
  api::PairingParams pairing_params;
  pairing_params.pairing_type =
      api::PairingParams::PairingType::kConfirmPasskey;
  pairing_params.passkey = "123456";
  env_.ConfigBluetoothPairingContext(&discovered_device->GetImpl(),
                                     pairing_params);

  // Initiates pairing request with remote device
  PairingError received_error = PairingError::kUnknown;
  CountDownLatch error_latch(1);
  CountDownLatch initiated_latch(1);
  EXPECT_TRUE(bluetooth_pairing->InitiatePairing({
      .on_pairing_error_cb =
          [&](api::BluetoothPairingCallback::PairingError error) {
            received_error = error;
            error_latch.CountDown();
          },
      .on_pairing_initiated_cb =
          [&](api::PairingParams pairingParams) {
            EXPECT_EQ(pairingParams.passkey, pairing_params.passkey);
            EXPECT_EQ(pairingParams.pairing_type, pairing_params.pairing_type);
            initiated_latch.CountDown();
          },
  }));
  initiated_latch.Await();

  // Cancels the ongoing pairing with remote device.
  EXPECT_TRUE(bluetooth_pairing->CancelPairing());
  error_latch.Await();
  EXPECT_EQ(received_error, PairingError::kAuthCanceled);
  EXPECT_FALSE(bluetooth_pairing->IsPaired());

  // Clear Bluetooth devices for pairing
  bluetooth_pairing.reset();
  EXPECT_FALSE(env_.SetPairingState(&discovered_device->GetImpl(), false));
}

}  // namespace
}  // namespace nearby
