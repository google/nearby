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

#include "fastpair/server_access/fast_pair_client_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "fastpair/common/fast_pair_switches.h"
#include "fastpair/server_access/fast_pair_http_notifier.h"
#include "internal/account/account_manager.h"
#include "internal/auth/auth_status_util.h"
#include "internal/auth/authentication_manager.h"
#include "internal/network/http_client.h"
#include "internal/network/http_request.h"
#include "internal/network/http_response.h"
#include "internal/network/url.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace {
using ::nearby::network::HttpRequest;
using ::nearby::network::HttpRequestMethod;
using ::nearby::network::HttpResponse;
using ::nearby::network::Url;
// ----------------- HTTP Constants ---------------------------------
constexpr absl::string_view kProtobufContentType = "application/x-protobuf";
constexpr absl::string_view kKey = "key";
constexpr absl::string_view kClientId =
    "AIzaSyBv7ZrOlX5oIJLVQrZh-WkZFKm5L6FlStQ";
constexpr absl::string_view kMode = "mode";
constexpr absl::string_view kQueryParameterAlternateOutputKey = "alt";
constexpr absl::string_view kQueryParameterAlternateOutputProto = "proto";
constexpr absl::string_view kPlatformTypeHeaderName =
    "X-FastPair-Platform-Type";
constexpr absl::string_view kDefaultNearbyDevicesHttpHost =
    "https://nearbydevices-pa.googleapis.com";

constexpr absl::string_view kNearbyV1Path = "v1/";
constexpr absl::string_view kDevicesPath = "device/";
constexpr absl::string_view kUserDevicesPath = "user/devices";
constexpr absl::string_view kUserDeleteDevicePath = "user/device";
const char* GetObservedDeviceMode[3] = {"MODE_UNKNOWN", "MODE_RELEASE",
                                        "MODE_DEBUG"};

absl::string_view GetPlatformTypeString(api::DeviceInfo::OsType os_type) {
  switch (os_type) {
    case api::DeviceInfo::OsType::kAndroid:
      return "OSType.ANDROID";
    case api::DeviceInfo::OsType::kChromeOs:
      return "OSType.CHROME_OS";
    case api::DeviceInfo::OsType::kIos:
      return "OSType.IOS";
    case api::DeviceInfo::OsType::kWindows:
      return "OSType.WINDOWS";
    default:
      return "OSType.UNKNOWN";
  }
}

// Creates the full Nearby V1 URL with |request_path|.
Url CreateV1RequestUrl(absl::string_view request_path) {
  std::string host = std::string(kDefaultNearbyDevicesHttpHost);
  auto host_switch = switches::GetNearbyFastPairHttpHost();
  if (!host_switch.empty()) {
    host = host_switch;
  }
  std::string path =
      absl::StrFormat("%s/%s%s", host, kNearbyV1Path, request_path);
  NEARBY_LOGS(INFO) << __func__ << "= " << path;
  return Url::Create(path).value();
}
}  // namespace

FastPairClientImpl::FastPairClientImpl(
    auth::AuthenticationManager* authentication_manager,
    AccountManager* account_manager, network::HttpClient* http_client,
    FastPairHttpNotifier* notifier, DeviceInfo* device_info)
    : authentication_manager_(authentication_manager),
      account_manager_(account_manager),
      http_client_(std::move(http_client)),
      notifier_(notifier),
      device_info_(device_info) {}

// Gets an observed device.
absl::StatusOr<proto::GetObservedDeviceResponse>
FastPairClientImpl::GetObservedDevice(
    const proto::GetObservedDeviceRequest& request) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Start API call to get observed device";
  notifier_->NotifyOfRequest(request);

  // Sets up request mode.
  QueryParameters params;
  params.push_back({std::string(kMode), GetObservedDeviceMode[request.mode()]});

  HttpRequest http_request = CreateHttpRequest(
      /*access token= */ std::nullopt,
      /*Url=*/
      CreateV1RequestUrl(std::string(kDevicesPath) +
                         std::to_string(request.device_id())),
      RequestType::kGet,
      /*query parameters=*/params,
      /*body=*/std::string());

  absl::StatusOr<HttpResponse> http_response =
      http_client_->GetResponse(http_request);

  if (!http_response.ok()) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get response.";
    return http_response.status();
  }

  proto::GetObservedDeviceResponse response;
  if (!response.ParseFromString(http_response->GetBody().GetRawData())) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to parse server response.";
    return absl::InvalidArgumentError("Parse proto error");
  }

  notifier_->NotifyOfResponse(response);
  NEARBY_LOGS(INFO)
      << __func__ << ": Complete API call to get observed device successfully";
  return response;
}

// Reads the user's devices.
absl::StatusOr<proto::UserReadDevicesResponse>
FastPairClientImpl::UserReadDevices(
    const proto::UserReadDevicesRequest& request) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Start API call to read user's devices";
  notifier_->NotifyOfRequest(request);

  absl::StatusOr<std::string> access_token = GetAccessToken();
  if (!access_token.ok()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Skip API call to read user devices due to no "
                            "available access token.";
    return absl::UnauthenticatedError("No user logged in.");
  }

  HttpRequest http_request = CreateHttpRequest(
      *access_token,
      /*Url=*/CreateV1RequestUrl(kUserDevicesPath), RequestType::kGet,
      /*query parameters=*/std::nullopt,
      /*body=*/request.SerializeAsString());

  absl::StatusOr<HttpResponse> http_response =
      http_client_->GetResponse(http_request);

  if (!http_response.ok()) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get response.";
    return http_response.status();
  }

  proto::UserReadDevicesResponse response;
  if (!response.ParseFromString(http_response->GetBody().GetRawData())) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to parse server response.";
    return absl::InvalidArgumentError("Parse proto error");
  }

  notifier_->NotifyOfResponse(response);
  NEARBY_LOGS(INFO) << __func__
                    << ": Complete API call to read user devices successfully";
  return response;
}

// Writes a new device to a user's account.
absl::StatusOr<proto::UserWriteDeviceResponse>
FastPairClientImpl::UserWriteDevice(
    const proto::UserWriteDeviceRequest& request) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Start API call to write user device";
  notifier_->NotifyOfRequest(request);

  absl::StatusOr<std::string> access_token = GetAccessToken();
  if (!access_token.ok()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Skip API call to write user devices due to no "
                            "available access token.";
    return absl::UnauthenticatedError("No user logged in.");
  }

  HttpRequest http_request = CreateHttpRequest(
      *access_token,
      /*Url=*/CreateV1RequestUrl(kUserDevicesPath), RequestType::kPost,
      /*query parameters=*/std::nullopt,
      /*body=*/request.SerializeAsString());
  absl::StatusOr<HttpResponse> http_response =
      http_client_->GetResponse(http_request);

  if (!http_response.ok()) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get response.";
    return http_response.status();
  }

  proto::UserWriteDeviceResponse response;
  if (!response.ParseFromString(http_response->GetBody().GetRawData())) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to parse server response.";
    return absl::InvalidArgumentError("Parse proto error");
  }

  notifier_->NotifyOfResponse(response);
  NEARBY_LOGS(INFO) << __func__
                    << ": Complete API call to write user devices successfully";
  return response;
}

// Deletes an existing device from a user's account.
absl::StatusOr<proto::UserDeleteDeviceResponse>
FastPairClientImpl::UserDeleteDevice(
    const proto::UserDeleteDeviceRequest& request) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Start API call to delete user device";
  notifier_->NotifyOfRequest(request);

  absl::StatusOr<std::string> access_token = GetAccessToken();
  if (!access_token.ok()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Skip API call to delete user devices due to no "
                            "available access token.";
    return absl::UnauthenticatedError("No user logged in.");
  }

  HttpRequest http_request =
      CreateHttpRequest(*access_token,
                        /*Url=*/
                        CreateV1RequestUrl(std::string(kUserDeleteDevicePath) +
                                           "/" + request.hex_account_key()),
                        RequestType::kDelete,
                        /*query parameters=*/std::nullopt,
                        /*body=*/std::string());

  absl::StatusOr<HttpResponse> http_response =
      http_client_->GetResponse(http_request);

  if (!http_response.ok()) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get response.";
    return http_response.status();
  }

  proto::UserDeleteDeviceResponse response;
  if (!response.ParseFromString(http_response->GetBody().GetRawData())) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to parse server response.";
    return absl::InvalidArgumentError("Parse proto error");
  }

  notifier_->NotifyOfResponse(response);
  NEARBY_LOGS(INFO)
      << __func__ << ": Complete API call to delete user devices successfully";
  return response;
}

// Blocking function
absl::StatusOr<std::string> FastPairClientImpl::GetAccessToken() {
  NEARBY_LOGS(VERBOSE) << __func__;
  std::optional<AccountManager::Account> account =
      account_manager_->GetCurrentAccount();
  if (!account.has_value()) {
    NEARBY_LOGS(WARNING)
        << __func__ << ": Failed to get access token due to no login user.";
    return absl::UnauthenticatedError("No user logged in.");
  }
  absl::StatusOr<std::string> result;
  absl::Notification notification;
  authentication_manager_->FetchAccessToken(
      account->id,
      [&](auth::AuthStatus status, absl::string_view access_token) {
        if (status == auth::AuthStatus::SUCCESS) {
          result = std::string(access_token);
        } else {
          result = absl::UnknownError(absl::StrCat(static_cast<int>(status)));
        }
        notification.Notify();
      });
  notification.WaitForNotification();
  return result;
}

HttpRequest FastPairClientImpl::CreateHttpRequest(
    std::optional<absl::string_view> access_token, Url url,
    RequestType request_type,
    std::optional<QueryParameters> request_as_query_parameters,
    std::optional<std::string> body) {
  NEARBY_LOGS(VERBOSE) << __func__;
  HttpRequest request{url};

  // Handles query strings
  request.AddQueryParameter(kQueryParameterAlternateOutputKey,
                            kQueryParameterAlternateOutputProto);
  request.AddQueryParameter(kKey, kClientId);
  if (request_as_query_parameters.has_value()) {
    for (const auto& key_value_pair : *request_as_query_parameters) {
      request.AddQueryParameter(key_value_pair.first, key_value_pair.second);
    }
  }

  // Handles request method
  request.SetMethod(GetRequestMethod(request_type));
  // Handles headers
  if (access_token.has_value()) {
    NEARBY_LOGS(INFO) << __func__ << " : Authorization with access token";
    request.AddHeader("Authorization",
                      absl::StrCat("Bearer ", access_token.value()));
  }
  request.AddHeader(kPlatformTypeHeaderName,
                    GetPlatformTypeString(device_info_->GetOsType()));
  request.AddHeader("Content-Type",
                    body ? kProtobufContentType : std::string());

  // Handles request body
  request.SetBody(body.value_or(std::string()));
  return request;
}

HttpRequestMethod FastPairClientImpl::GetRequestMethod(
    RequestType request_type) const {
  if (request_type == RequestType::kPost) {
    NEARBY_LOGS(INFO) << __func__ << " : request_type= kPost";
    return HttpRequestMethod::kPost;
  } else if (request_type == RequestType::kDelete) {
    NEARBY_LOGS(INFO) << __func__ << ": request_type= kDelete";
    return HttpRequestMethod::kDelete;
  }
  NEARBY_LOGS(INFO) << __func__ << " : request_type= kGet";
  return HttpRequestMethod::kGet;
}
}  // namespace fastpair
}  // namespace nearby
