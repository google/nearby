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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAKE_FAST_PAIR_CLIENT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAKE_FAST_PAIR_CLIENT_H_

#include <optional>

#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/server_access/fast_pair_client.h"

namespace nearby {
namespace fastpair {

// A fake implementation of the FastPairClient that stores all request
// data. Only use in unit tests.
class FakeFastPairClient : public FastPairClient {
 public:
  FakeFastPairClient() = default;
  ~FakeFastPairClient() override = default;
  proto::GetObservedDeviceRequest& get_observer_device_request() {
    return get_observer_device_request_.value();
  }

  proto::GetObservedDeviceResponse& get_observer_device_response() {
    return get_observer_device_response_.value();
  }

  proto::UserReadDevicesRequest& read_devices_request() {
    return read_devices_request_.value();
  }

  proto::UserReadDevicesResponse& read_devices_response() {
    return read_devices_response_.value();
  }

  proto::UserWriteDeviceRequest& write_device_request() {
    return write_device_request_.value();
  }

  proto::UserDeleteDeviceRequest& delete_device_request() {
    return delete_device_request_.value();
  }

  void SetGetObservedDeviceResponse(
      absl::StatusOr<proto::GetObservedDeviceResponse> response) {
    get_observer_device_response_ = response;
  }

  void SetUserReadDevicesResponse(
      absl::StatusOr<proto::UserReadDevicesResponse> responses) {
    read_devices_response_ = responses;
  }

  void SetUserWriteDeviceResponse(
      absl::StatusOr<proto::UserWriteDeviceResponse> response) {
    write_device_response_ = response;
  }

  void SetUserDeleteDeviceResponse(
      absl::StatusOr<proto::UserDeleteDeviceResponse> response) {
    delete_device_response_ = response;
  }

 private:
  // Gets an observed device.
  // Blocking function
  absl::StatusOr<proto::GetObservedDeviceResponse> GetObservedDevice(
      const proto::GetObservedDeviceRequest& request) override {
    get_observer_device_request_ = request;
    return get_observer_device_response_;
  }

  // Reads the user's devices.
  // Blocking function
  absl::StatusOr<proto::UserReadDevicesResponse> UserReadDevices(
      const proto::UserReadDevicesRequest& request) override {
    read_devices_request_ = request;
    return read_devices_response_;
  }

  // Writes a new device to a user's account.
  // Blocking function
  absl::StatusOr<proto::UserWriteDeviceResponse> UserWriteDevice(
      const proto::UserWriteDeviceRequest& request) override {
    write_device_request_ = request;
    return write_device_response_;
  }

  // Deletes an existing device from a user's account.
  // Blocking function
  absl::StatusOr<proto::UserDeleteDeviceResponse> UserDeleteDevice(
      const proto::UserDeleteDeviceRequest& request) override {
    delete_device_request_ = request;
    return delete_device_response_;
  }

  // Requests/Responses
  std::optional<proto::GetObservedDeviceRequest> get_observer_device_request_;
  absl::StatusOr<proto::GetObservedDeviceResponse>
      get_observer_device_response_;
  std::optional<proto::UserReadDevicesRequest> read_devices_request_;
  absl::StatusOr<proto::UserReadDevicesResponse> read_devices_response_;
  std::optional<proto::UserWriteDeviceRequest> write_device_request_;
  absl::StatusOr<proto::UserWriteDeviceResponse> write_device_response_;
  std::optional<proto::UserDeleteDeviceRequest> delete_device_request_;
  absl::StatusOr<proto::UserDeleteDeviceResponse> delete_device_response_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAKE_FAST_PAIR_CLIENT_H_
