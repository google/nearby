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

#include "fastpair/server_access/fast_pair_http_notifier.h"

#include "gtest/gtest.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {

namespace {
class FastPairHttpNotifierObserver : public FastPairHttpNotifier::Observer {
 public:
  explicit FastPairHttpNotifierObserver(CountDownLatch* latch) {
    latch_ = latch;
  }

  void OnGetObservedDeviceRequest(
      const proto::GetObservedDeviceRequest& request) override {
    latch_->CountDown();
    get_observer_device_request_ =
        &const_cast<proto::GetObservedDeviceRequest&>(request);
  }

  void OnGetObservedDeviceResponse(
      const proto::GetObservedDeviceResponse& response) override {
    latch_->CountDown();
    get_observer_device_response_ =
        &const_cast<proto::GetObservedDeviceResponse&>(response);
  }

  // Called when HTTP RPC is made for UserReadDevicesRequest/Response
  void OnUserReadDevicesRequest(
      const proto::UserReadDevicesRequest& request) override {
    latch_->CountDown();
    read_devices_request_ =
        &const_cast<proto::UserReadDevicesRequest&>(request);
  }
  void OnUserReadDevicesResponse(
      const proto::UserReadDevicesResponse& response) override {
    latch_->CountDown();
    read_devices_response_ =
        &const_cast<proto::UserReadDevicesResponse&>(response);
  }

  // Called when HTTP RPC is made for UserWriteDeviceRequest/Response
  void OnUserWriteDeviceRequest(
      const proto::UserWriteDeviceRequest& request) override {
    latch_->CountDown();
    write_device_request_ =
        &const_cast<proto::UserWriteDeviceRequest&>(request);
  }
  void OnUserWriteDeviceResponse(
      const proto::UserWriteDeviceResponse& response) override {
    latch_->CountDown();
    write_device_response_ =
        &const_cast<proto::UserWriteDeviceResponse&>(response);
  }

  // Called when HTTP RPC is made for UserDeleteDeviceRequest/Response
  void OnUserDeleteDeviceRequest(
      const proto::UserDeleteDeviceRequest& request) override {
    latch_->CountDown();
    delete_device_request_ =
        &const_cast<proto::UserDeleteDeviceRequest&>(request);
  }
  void OnUserDeleteDeviceResponse(
      const proto::UserDeleteDeviceResponse& response) override {
    latch_->CountDown();
    delete_device_response_ =
        &const_cast<proto::UserDeleteDeviceResponse&>(response);
  }

  CountDownLatch* latch_;
  proto::GetObservedDeviceRequest* get_observer_device_request_;
  proto::GetObservedDeviceResponse* get_observer_device_response_;
  proto::UserReadDevicesRequest* read_devices_request_;
  proto::UserReadDevicesResponse* read_devices_response_;
  proto::UserWriteDeviceRequest* write_device_request_;
  proto::UserWriteDeviceResponse* write_device_response_;
  proto::UserDeleteDeviceRequest* delete_device_request_;
  proto::UserDeleteDeviceResponse* delete_device_response_;
};

TEST(FastPairHttpNotifierTest, TestNotifyGetObservedDeviceRequest) {
  proto::GetObservedDeviceRequest request;
  int64_t device_id;
  CHECK(absl::SimpleHexAtoi("718C17", &device_id));
  request.set_device_id(device_id);
  request.set_mode(proto::GetObservedDeviceRequest::MODE_RELEASE);
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfRequest(request);
  latch.Await();

  EXPECT_EQ(observer.get_observer_device_request_, &request);
  EXPECT_EQ(observer.get_observer_device_request_->device_id(), device_id);
  EXPECT_EQ(observer.get_observer_device_request_->mode(),
            proto::GetObservedDeviceRequest::MODE_RELEASE);
}

TEST(FastPairHttpNotifierTest, TestNotifyGetObservedDeviceResponse) {
  proto::GetObservedDeviceResponse response;
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfResponse(response);
  latch.Await();

  EXPECT_EQ(observer.get_observer_device_response_, &response);
}

TEST(FastPairHttpNotifierTest, TestNotifyUserReadDevicesRequest) {
  proto::UserReadDevicesRequest request;
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfRequest(request);
  latch.Await();

  EXPECT_EQ(observer.read_devices_request_, &request);
}

TEST(FastPairHttpNotifierTest, TestNotifyUserReadDevicesResponse) {
  proto::UserReadDevicesResponse response;
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfResponse(response);
  latch.Await();

  EXPECT_EQ(observer.read_devices_response_, &response);
}

TEST(FastPairHttpNotifierTest, TestNotifyUserWritedeviceRequest) {
  proto::UserWriteDeviceRequest request;
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfRequest(request);
  latch.Await();

  EXPECT_EQ(observer.write_device_request_, &request);
}

TEST(FastPairHttpNotifierTest, TestNotifyUserWriteDeviceResponse) {
  proto::UserWriteDeviceResponse response;
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfResponse(response);
  latch.Await();

  EXPECT_EQ(observer.write_device_response_, &response);
}

TEST(FastPairHttpNotifierTest, TestNotifyUserDeleteDeviceRequest) {
  proto::UserDeleteDeviceRequest request;
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfRequest(request);
  latch.Await();

  EXPECT_EQ(observer.delete_device_request_, &request);
}

TEST(FastPairHttpNotifierTest, TestNotifyUserDeleteDeviceResponse) {
  proto::UserDeleteDeviceResponse response;
  CountDownLatch latch(1);
  FastPairHttpNotifierObserver observer(&latch);
  FastPairHttpNotifier notifier;
  notifier.AddObserver(&observer);

  notifier.NotifyOfResponse(response);
  latch.Await();

  EXPECT_EQ(observer.delete_device_response_, &response);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
