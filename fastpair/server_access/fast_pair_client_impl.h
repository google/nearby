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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_CLIENT_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "fastpair/server_access/fast_pair_client.h"
#include "fastpair/server_access/fast_pair_http_notifier.h"
#include "internal/account/account_manager.h"
#include "internal/auth/authentication_manager.h"
#include "internal/network/http_client.h"
#include "internal/network/url.h"
#include "internal/platform/device_info.h"

namespace nearby {
namespace fastpair {
// An implementation of FastPairClient that fetches access tokens and makes
// HTTP request to FastPair backend.
class FastPairClientImpl : public FastPairClient {
 public:
  // Query strings of the request.
  using QueryParameters = std::vector<std::pair<std::string, std::string>>;

  FastPairClientImpl(auth::AuthenticationManager* authentication_manager,
                     AccountManager* account_manager,
                     network::HttpClient* http_client,
                     FastPairHttpNotifier* notifier, DeviceInfo* device_info);
  FastPairClientImpl(FastPairClientImpl&) = delete;
  FastPairClientImpl& operator=(FastPairClientImpl&) = delete;
  ~FastPairClientImpl() override = default;

  // Gets an observed device.
  absl::StatusOr<proto::GetObservedDeviceResponse> GetObservedDevice(
      const proto::GetObservedDeviceRequest& request) override;

  // Reads the user's devices.
  absl::StatusOr<proto::UserReadDevicesResponse> UserReadDevices(
      const proto::UserReadDevicesRequest& request) override;

  // Writes a new device to a user's account.
  absl::StatusOr<proto::UserWriteDeviceResponse> UserWriteDevice(
      const proto::UserWriteDeviceRequest& request) override;

  // Deletes an existing device from a user's account.
  absl::StatusOr<proto::UserDeleteDeviceResponse> UserDeleteDevice(
      const proto::UserDeleteDeviceRequest& request) override;

 private:
  enum class RequestType { kGet, kPost, kDelete };
  network::HttpRequestMethod GetRequestMethod(
      FastPairClientImpl::RequestType request_type) const;
  // Fetches access token for a logged in user.
  absl::StatusOr<std::string> GetAccessToken();
  network::HttpRequest CreateHttpRequest(
      std::optional<absl::string_view> access_token, network::Url url,
      RequestType request_type,
      std::optional<QueryParameters> request_as_query_parameters,
      std::optional<std::string> body);

  auth::AuthenticationManager* authentication_manager_ = nullptr;
  AccountManager* account_manager_ = nullptr;
  network::HttpClient* http_client_;
  FastPairHttpNotifier* notifier_ = nullptr;
  DeviceInfo* device_info_ = nullptr;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_CLIENT_IMPL_H_
