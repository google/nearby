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

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/fast_pair_switches.h"
#include "fastpair/common/protocol.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/enum.proto.h"
#include "fastpair/proto/fast_pair_string.proto.h"
#include "fastpair/proto/proto_builder.h"
#include "fastpair/server_access/fast_pair_client.h"
#include "fastpair/server_access/fast_pair_http_notifier.h"
#include "internal/auth/auth_status_util.h"
#include "internal/auth/authentication_manager.h"
#include "internal/network/http_client.h"
#include "internal/network/http_request.h"
#include "internal/network/http_response.h"
#include "internal/network/http_status_code.h"
#include "internal/network/url.h"
#include "internal/platform/device_info.h"
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/google3_only/fake_authentication_manager.h"

namespace nearby {
namespace fastpair {
namespace {

using ::nearby::network::HttpClient;
using ::nearby::network::HttpRequest;
using ::nearby::network::HttpRequestMethod;
using ::nearby::network::HttpResponse;
using ::nearby::network::HttpStatusCode;
using ::nearby::network::Url;

constexpr absl::string_view kHexModelId = "718C17";
constexpr absl::string_view kAccessToken = "access_token";
constexpr absl::string_view kTestAccountId = "test_account_id";
constexpr absl::string_view kFastPairPreferencesFilePath =
    "Google/Nearby/FastPair";
constexpr absl::string_view kDevicesPath = "device/";
constexpr absl::string_view kUserDevicesPath = "user/devices";
constexpr absl::string_view kUserDeleteDevicePath = "user/device";
constexpr absl::string_view kKey = "key";
constexpr absl::string_view kClientId =
    "AIzaSyBv7ZrOlX5oIJLVQrZh-WkZFKm5L6FlStQ";
constexpr absl::string_view kQueryParameterAlternateOutputKey = "alt";
constexpr absl::string_view kQueryParameterAlternateOutputProto = "proto";
constexpr absl::string_view kPlatformTypeHeaderName =
    "X-FastPair-Platform-Type";
constexpr absl::string_view kWindowsPlatformType = "OSType.WINDOWS";
constexpr absl::string_view kMode = "mode";
constexpr absl::string_view kReleaseMode = "MODE_RELEASE";
constexpr absl::string_view kTestGoogleApisUrl =
    "https://nearbydevices-pa.testgoogleapis.com";
constexpr absl::string_view kBleAddress = "11::22::33::44::55::66";
constexpr absl::string_view kPublicAddress = "20:64:DE:40:F8:93";
constexpr absl::string_view kDisplayName = "Test Device";
constexpr absl::string_view kInitialPairingdescription =
    "InitialPairingdescription";
constexpr absl::string_view kAccountKey = "04b85786180add47fb81a04a8ce6b0de";
constexpr absl::string_view kExpectedSha256Hash =
    "6353c0075a35b7d81bb30a6190ab246da4b8c55a6111d387400579133c090ed8";

class MockHttpClient : public HttpClient {
 public:
  MOCK_METHOD(void, StartRequest,
              (const HttpRequest& request,
               absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)>),
              (override));
  MOCK_METHOD(void, StartCancellableRequest,
              (std::unique_ptr<CancellableRequest> request,
               absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)>),
              (override));
  MOCK_METHOD(absl::StatusOr<HttpResponse>, GetResponse, (const HttpRequest&),
              (override));
};

// Return the values associated with |key|, or fail the test if |key| isn't in
// |query_parameters|
std::vector<std::string> ExpectQueryStringValues(
    const std::vector<std::pair<std::string, std::string>>& query_parameters,
    absl::string_view key) {
  std::vector<std::string> values;
  for (const auto& pair : query_parameters) {
    const auto& query_key = pair.first;
    const auto& query_value = pair.second;
    if (query_key == key) {
      values.push_back(query_value);
    }
  }
  EXPECT_GT(values.size(), 0);
  return values;
}

// A gMock matcher to match proto values. Use this matcher like:
// request/response proto, expected_proto;
// EXPECT_THAT(proto, MatchesProto(expected_proto));
MATCHER_P(
    MatchesProto, expected_proto,
    absl::StrCat(negation ? "does not match" : "matches",
                 testing::PrintToString(expected_proto.SerializeAsString()))) {
  return arg.has_value() &&
         arg->SerializeAsString() == expected_proto.SerializeAsString();
}

class FastPairClientImplTest : public ::testing::Test,
                               public FastPairHttpNotifier::Observer {
 protected:
  FastPairClientImplTest() {
    authentication_manager_ = std::make_unique<FakeAuthenticationManager>();
    AccountManager::Account account;
    account.id = kTestAccountId;
    account_manager_ = std::make_unique<FakeAccountManager>();
    account_manager_->SetAccount(account);
    device_info_ = std::make_unique<FakeDeviceInfo>();
  }

  void SetUp() override {
    GetAuthManager()->EnableSyncMode();
    mock_http_client_ = std::make_unique<::testing::NiceMock<MockHttpClient>>();
    http_client_ = dynamic_cast<::testing::NiceMock<MockHttpClient>*>(
        mock_http_client_.get());
    switches::SetNearbyFastPairHttpHost(std::string(kTestGoogleApisUrl));
    fast_pair_client_ = std::make_unique<FastPairClientImpl>(
        authentication_manager_.get(), account_manager_.get(),
        mock_http_client_.get(), &notifier_, device_info_.get());
    notifier_.AddObserver(this);
  }

  void TearDown() override { notifier_.RemoveObserver(this); }

  nearby::FakeAuthenticationManager* GetAuthManager() {
    return reinterpret_cast<nearby::FakeAuthenticationManager*>(
        authentication_manager_.get());
  }

  // FastPairHttpNotifier::Observer:
  // Called when HTTP RPC is made for GetObservedDeviceRequest/Response
  void OnGetObservedDeviceRequest(
      const proto::GetObservedDeviceRequest& request) override {
    get_observer_device_request_ = request;
  }

  void OnGetObservedDeviceResponse(
      const proto::GetObservedDeviceResponse& response) override {
    get_observer_device_response_ = response;
  }

  // Called when HTTP RPC is made for UserReadDevicesRequest/Response
  void OnUserReadDevicesRequest(
      const proto::UserReadDevicesRequest& request) override {
    read_devices_request_ = request;
  }
  void OnUserReadDevicesResponse(
      const proto::UserReadDevicesResponse& response) override {
    read_devices_response_ = response;
  }

  // Called when HTTP RPC is made for UserWriteDeviceRequest/Response
  void OnUserWriteDeviceRequest(
      const proto::UserWriteDeviceRequest& request) override {
    write_device_request_ = request;
  }
  void OnUserWriteDeviceResponse(
      const proto::UserWriteDeviceResponse& response) override {
    write_device_response_ = response;
  }

  // Called when HTTP RPC is made for UserDeleteDeviceRequest/Response
  void OnUserDeleteDeviceRequest(
      const proto::UserDeleteDeviceRequest& request) override {
    delete_device_request_ = request;
  }
  void OnUserDeleteDeviceResponse(
      const proto::UserDeleteDeviceResponse& response) override {
    delete_device_response_ = response;
  }

  Url GetUrl(absl::string_view path) {
    return Url::Create(absl::StrCat(kTestGoogleApisUrl, "/v1/", path)).value();
  }

  // Requests/Responses
  std::optional<proto::GetObservedDeviceRequest> get_observer_device_request_;
  std::optional<proto::GetObservedDeviceResponse> get_observer_device_response_;
  std::optional<proto::UserReadDevicesRequest> read_devices_request_;
  std::optional<proto::UserReadDevicesResponse> read_devices_response_;
  std::optional<proto::UserWriteDeviceRequest> write_device_request_;
  std::optional<proto::UserWriteDeviceResponse> write_device_response_;
  std::optional<proto::UserDeleteDeviceRequest> delete_device_request_;
  std::optional<proto::UserDeleteDeviceResponse> delete_device_response_;

  std::unique_ptr<auth::AuthenticationManager> authentication_manager_;
  std::unique_ptr<FakeAccountManager> account_manager_;
  std::unique_ptr<FastPairClient> fast_pair_client_;
  std::unique_ptr<DeviceInfo> device_info_;
  ::testing::NiceMock<MockHttpClient>* http_client_;
  std::unique_ptr<MockHttpClient> mock_http_client_;
  FastPairHttpNotifier notifier_;
};

TEST_F(FastPairClientImplTest, GetObservedDeviceSuccess) {
  // Sets up proto::GetObservedDeviceRequest
  proto::GetObservedDeviceRequest request_proto;
  int64_t device_id;
  CHECK(absl::SimpleHexAtoi(kHexModelId, &device_id));
  request_proto.set_device_id(device_id);
  request_proto.set_mode(proto::GetObservedDeviceRequest::MODE_RELEASE);

  // Sets up proto::GetObservedDeviceResponse
  proto::GetObservedDeviceResponse response_proto;
  auto* device = response_proto.mutable_device();
  device->set_id(device_id);
  auto* observed_device_strings = response_proto.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);

  // Sets up HttpResponse
  HttpResponse http_response;
  http_response.SetStatusCode(nearby::network::HttpStatusCode::kHttpOk);
  http_response.SetBody(response_proto.SerializeAsString());

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kGet);
        EXPECT_THAT(request.GetUrl().GetUrlPath(),
                    ::testing::HasSubstr(GetUrl(kDevicesPath).GetUrlPath()));

        EXPECT_EQ(request.GetAllHeaders().find(kPlatformTypeHeaderName)->second,
                  std::vector<std::string>{std::string(kWindowsPlatformType)});
        EXPECT_EQ(
            ExpectQueryStringValues(request.GetAllQueryParameters(), kKey),
            std::vector<std::string>{std::string(kClientId)});
        EXPECT_EQ(ExpectQueryStringValues(request.GetAllQueryParameters(),
                                          kQueryParameterAlternateOutputKey),
                  std::vector<std::string>{
                      std::string(kQueryParameterAlternateOutputProto)});
        EXPECT_EQ(
            ExpectQueryStringValues(request.GetAllQueryParameters(), kMode),
            std::vector<std::string>{std::string(kReleaseMode)});
        proto::GetObservedDeviceRequest expected_request;
        EXPECT_TRUE(expected_request.ParseFromString(
            request_proto.SerializeAsString()));
        EXPECT_EQ(expected_request.device_id(), device_id);
        EXPECT_EQ(expected_request.mode(),
                  proto::GetObservedDeviceRequest::MODE_RELEASE);
        return http_response;
      });

  absl::StatusOr<proto::GetObservedDeviceResponse> response =
      fast_pair_client_->GetObservedDevice(request_proto);

  EXPECT_OK(response);

  // Verifies proto::GetObservedDeviceRequest is as expected
  EXPECT_THAT(get_observer_device_request_, MatchesProto(request_proto));

  // Verifies proto::GetObservedDeviceResponse is as expected
  EXPECT_THAT(get_observer_device_response_, MatchesProto(response_proto));
  EXPECT_EQ(response->device().id(), device_id);
  EXPECT_EQ(response->strings().initial_pairing_description(),
            kInitialPairingdescription);
}

TEST_F(FastPairClientImplTest, GetObservedDeviceFailureWhenNoRespsone) {
  // Sets up proto::GetObservedDeviceRequest
  proto::GetObservedDeviceRequest request_proto;
  int64_t device_id;
  CHECK(absl::SimpleHexAtoi(kHexModelId, &device_id));
  request_proto.set_device_id(device_id);
  request_proto.set_mode(proto::GetObservedDeviceRequest::MODE_RELEASE);

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kGet);
        EXPECT_THAT(request.GetUrl().GetUrlPath(),
                    ::testing::HasSubstr(GetUrl(kDevicesPath).GetUrlPath()));
        return absl::UnavailableError("");
      });

  absl::StatusOr<proto::GetObservedDeviceResponse> response =
      fast_pair_client_->GetObservedDevice(request_proto);

  EXPECT_FALSE(response.ok());
  EXPECT_TRUE(absl::IsUnavailable(response.status()));
}

TEST_F(FastPairClientImplTest, GetObservedDeviceFailureWhenParseResponse) {
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kGet);
        HttpResponse http_response;
        http_response.SetStatusCode(HttpStatusCode::kHttpOk);
        http_response.SetBody("Not a valid serialized response message.");
        return http_response;
      });

  absl::StatusOr<proto::GetObservedDeviceResponse> response =
      fast_pair_client_->GetObservedDevice(proto::GetObservedDeviceRequest());
  EXPECT_TRUE(absl::IsInvalidArgument(response.status()));
}

TEST_F(FastPairClientImplTest, UserReadDevicesSuccess) {
  // Sets up proto::UserReadDevicesRequest
  proto::UserReadDevicesRequest request_proto;

  // Sets up proto::UserReadDevicesResponse
  proto::UserReadDevicesResponse response_proto;
  auto* fast_pair_info = response_proto.add_fast_pair_info();
  fast_pair_info->set_opt_in_status(proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);

  // Sets up HttpResponse
  HttpResponse http_response;
  http_response.SetStatusCode(nearby::network::HttpStatusCode::kHttpOk);
  http_response.SetBody(response_proto.SerializeAsString());

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kGet);
        EXPECT_THAT(
            request.GetUrl().GetUrlPath(),
            ::testing::HasSubstr(GetUrl(kUserDevicesPath).GetUrlPath()));
        return http_response;
      });

  absl::StatusOr<proto::UserReadDevicesResponse> response =
      fast_pair_client_->UserReadDevices(request_proto);

  EXPECT_OK(response);

  // Verifies proto::UserReadDevicesRequest is as expected
  EXPECT_THAT(read_devices_request_, MatchesProto(request_proto));

  // Verifies proto::UserReadDevicesResponse is as expected
  EXPECT_THAT(read_devices_response_, MatchesProto(response_proto));
  EXPECT_EQ(response->fast_pair_info().size(), 1);
  EXPECT_EQ(response->fast_pair_info().Get(0).opt_in_status(),
            proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);
}

TEST_F(FastPairClientImplTest, UserReadDevicesFailureWhenNoRespsone) {
  // Sets up proto::UserReadDevicesRequest
  proto::UserReadDevicesRequest request_proto;

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kGet);
        EXPECT_THAT(
            request.GetUrl().GetUrlPath(),
            ::testing::HasSubstr(GetUrl(kUserDevicesPath).GetUrlPath()));
        return absl::UnavailableError("");
      });

  absl::StatusOr<proto::UserReadDevicesResponse> response =
      fast_pair_client_->UserReadDevices(request_proto);

  EXPECT_FALSE(response.ok());
  EXPECT_TRUE(absl::IsUnavailable(response.status()));
}

TEST_F(FastPairClientImplTest, UserReadDevicesFailureWhenNoLoginUser) {
  account_manager_->SetAccount(std::nullopt);

  EXPECT_CALL(*http_client_, GetResponse).Times(0);
  absl::StatusOr<proto::UserReadDevicesResponse> response =
      fast_pair_client_->UserReadDevices(proto::UserReadDevicesRequest());
  EXPECT_FALSE(response.ok());
}

TEST_F(FastPairClientImplTest, UserReadDevicesFailureWhenParseResponse) {
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kGet);
        HttpResponse http_response;
        http_response.SetStatusCode(HttpStatusCode::kHttpOk);
        http_response.SetBody("Not a valid serialized response message.");
        return http_response;
      });

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));
  absl::StatusOr<proto::UserReadDevicesResponse> response =
      fast_pair_client_->UserReadDevices(proto::UserReadDevicesRequest());
  EXPECT_TRUE(absl::IsInvalidArgument(response.status()));
}

TEST_F(FastPairClientImplTest, UserWriteDeviceSuccess) {
  // Sets up proto::UserWriteDeviceRequest
  proto::UserWriteDeviceRequest request_proto;
  FastPairDevice device(kHexModelId, kBleAddress,
                        Protocol::kFastPairInitialPairing);
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));
  device.SetAccountKey(account_key);
  device.SetPublicAddress(kPublicAddress);
  device.SetDisplayName(kDisplayName);
  proto::GetObservedDeviceResponse get_observed_device_response;
  auto* observed_device_strings =
      get_observed_device_response.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata(get_observed_device_response);
  device.SetMetadata(device_metadata);
  auto* fast_pair_info = request_proto.mutable_fast_pair_info();
  BuildFastPairInfo(fast_pair_info, device);

  // Sets up proto::UserWriteDeviceResponse
  proto::UserWriteDeviceResponse response_proto;

  // Sets up HttpResponse
  HttpResponse http_response;
  http_response.SetStatusCode(nearby::network::HttpStatusCode::kHttpOk);
  http_response.SetBody(response_proto.SerializeAsString());

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kPost);
        EXPECT_THAT(
            request.GetUrl().GetUrlPath(),
            ::testing::HasSubstr(GetUrl(kUserDevicesPath).GetUrlPath()));
        proto::UserWriteDeviceRequest expected_request;
        EXPECT_TRUE(expected_request.ParseFromString(
            request_proto.SerializeAsString()));
        EXPECT_TRUE(expected_request.has_fast_pair_info());
        auto fast_proto_info = expected_request.fast_pair_info();
        EXPECT_TRUE(fast_proto_info.has_device());
        auto device = fast_proto_info.device();
        EXPECT_EQ(device.account_key(), account_key.GetAsBytes());
        EXPECT_EQ(
            absl::BytesToHexString(device.sha256_account_key_public_address()),
            kExpectedSha256Hash);
        proto::StoredDiscoveryItem stored_discovery_item;
        EXPECT_TRUE(stored_discovery_item.ParseFromString(
            device.discovery_item_bytes()));
        EXPECT_EQ(stored_discovery_item.title(), kDisplayName);
        proto::FastPairStrings fast_pair_strings =
            stored_discovery_item.fast_pair_strings();
        EXPECT_EQ(fast_pair_strings.initial_pairing_description(),
                  kInitialPairingdescription);
        return http_response;
      });

  absl::StatusOr<proto::UserWriteDeviceResponse> response =
      fast_pair_client_->UserWriteDevice(request_proto);

  EXPECT_OK(response);

  // Verifies proto::UserWriteDeviceRequest is as expected
  EXPECT_THAT(write_device_request_, MatchesProto(request_proto));

  // Verifies proto::UserWriteDeviceResponse is as expected
  EXPECT_THAT(write_device_response_, MatchesProto(response_proto));
}

TEST_F(FastPairClientImplTest, UserWriteDeviceFailureWhenNoRespsone) {
  // Sets up proto::UserWriteDeviceRequest
  proto::UserWriteDeviceRequest request_proto;

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kPost);
        EXPECT_THAT(
            request.GetUrl().GetUrlPath(),
            ::testing::HasSubstr(GetUrl(kUserDevicesPath).GetUrlPath()));
        return absl::UnavailableError("");
      });

  absl::StatusOr<proto::UserWriteDeviceResponse> response =
      fast_pair_client_->UserWriteDevice(request_proto);

  EXPECT_FALSE(response.ok());
  EXPECT_TRUE(absl::IsUnavailable(response.status()));
}

TEST_F(FastPairClientImplTest, UserWriteDeviceFailureWhenNoLoginUser) {
  account_manager_->SetAccount(std::nullopt);

  EXPECT_CALL(*http_client_, GetResponse).Times(0);
  absl::StatusOr<proto::UserWriteDeviceResponse> response =
      fast_pair_client_->UserWriteDevice(proto::UserWriteDeviceRequest());
  EXPECT_FALSE(response.ok());
}

TEST_F(FastPairClientImplTest, UserWriteDeviceFailureWhenParseResponse) {
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kPost);
        HttpResponse http_response;
        http_response.SetStatusCode(HttpStatusCode::kHttpOk);
        http_response.SetBody("Not a valid serialized response message.");
        return http_response;
      });

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));
  absl::StatusOr<proto::UserWriteDeviceResponse> response =
      fast_pair_client_->UserWriteDevice(proto::UserWriteDeviceRequest());
  EXPECT_TRUE(absl::IsInvalidArgument(response.status()));
}

TEST_F(FastPairClientImplTest, UserDeleteDeviceSuccess) {
  // Sets up proto::UserDeleteDeviceRequest
  std::string hex_account_key = std::string(kAccountKey);
  absl::AsciiStrToUpper(&hex_account_key);
  proto::UserDeleteDeviceRequest request_proto;
  request_proto.set_hex_account_key(hex_account_key);

  // Sets up proto::UserDeleteDeviceResponse
  proto::UserDeleteDeviceResponse response_proto;

  // Sets up HttpResponse
  HttpResponse http_response;
  http_response.SetStatusCode(nearby::network::HttpStatusCode::kHttpOk);
  http_response.SetBody(response_proto.SerializeAsString());

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kDelete);
        EXPECT_THAT(
            request.GetUrl().GetUrlPath(),
            ::testing::HasSubstr(GetUrl(kUserDeleteDevicePath).GetUrlPath()));
        proto::UserDeleteDeviceRequest expected_request;
        EXPECT_TRUE(expected_request.ParseFromString(
            request_proto.SerializeAsString()));
        EXPECT_EQ(expected_request.hex_account_key(), hex_account_key);
        return http_response;
      });

  absl::StatusOr<proto::UserDeleteDeviceResponse> response =
      fast_pair_client_->UserDeleteDevice(request_proto);

  EXPECT_OK(response);

  // Verifies proto::UserDeleteDeviceRequest is as expected
  EXPECT_THAT(delete_device_request_, MatchesProto(request_proto));

  // Verifies proto::UserDeleteDeviceResponse is as expected
  EXPECT_THAT(delete_device_response_, MatchesProto(response_proto));
}

TEST_F(FastPairClientImplTest, UserDeleteDeviceFailureWhenNoRespsone) {
  // Sets up proto::UserDeleteDeviceRequest
  proto::UserDeleteDeviceRequest request_proto;

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));

  // Verifies HttpRequest is as expected
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kDelete);
        EXPECT_THAT(
            request.GetUrl().GetUrlPath(),
            ::testing::HasSubstr(GetUrl(kUserDeleteDevicePath).GetUrlPath()));
        return absl::UnavailableError("");
      });

  absl::StatusOr<proto::UserDeleteDeviceResponse> response =
      fast_pair_client_->UserDeleteDevice(request_proto);

  EXPECT_FALSE(response.ok());
  EXPECT_TRUE(absl::IsUnavailable(response.status()));
}

TEST_F(FastPairClientImplTest, UserDeleteDeviceFailureWhenNoLoginUser) {
  account_manager_->SetAccount(std::nullopt);

  EXPECT_CALL(*http_client_, GetResponse).Times(0);
  absl::StatusOr<proto::UserDeleteDeviceResponse> response =
      fast_pair_client_->UserDeleteDevice(proto::UserDeleteDeviceRequest());
  EXPECT_FALSE(response.ok());
}

TEST_F(FastPairClientImplTest, UserDeleteDeviceFailureWhenParseResponse) {
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kDelete);
        HttpResponse http_response;
        http_response.SetStatusCode(HttpStatusCode::kHttpOk);
        http_response.SetBody("Not a valid serialized response message.");
        return http_response;
      });

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));
  absl::StatusOr<proto::UserDeleteDeviceResponse> response =
      fast_pair_client_->UserDeleteDevice(proto::UserDeleteDeviceRequest());
  EXPECT_TRUE(absl::IsInvalidArgument(response.status()));
}

TEST_F(FastPairClientImplTest, FetchAccessTokenFailure) {
  EXPECT_CALL(*http_client_, GetResponse).Times(0);

  GetAuthManager()->SetFetchAccessTokenResult(
      auth::AuthStatus::PERMISSION_DENIED, std::nullopt);
  absl::StatusOr<proto::UserReadDevicesResponse> response =
      fast_pair_client_->UserReadDevices(proto::UserReadDevicesRequest());

  EXPECT_TRUE(absl::IsUnauthenticated(response.status()));
}

TEST_F(FastPairClientImplTest, ParseResponseProtoFailure) {
  EXPECT_CALL(*http_client_, GetResponse)
      .WillOnce([&](const HttpRequest& request) {
        EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kGet);
        HttpResponse http_response;
        http_response.SetStatusCode(HttpStatusCode::kHttpOk);
        http_response.SetBody("Not a valid serialized response message.");
        return http_response;
      });

  GetAuthManager()->SetFetchAccessTokenResult(auth::AuthStatus::SUCCESS,
                                              std::string(kAccessToken));
  absl::StatusOr<proto::UserReadDevicesResponse> response =
      fast_pair_client_->UserReadDevices(proto::UserReadDevicesRequest());
  EXPECT_TRUE(absl::IsInvalidArgument(response.status()));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
