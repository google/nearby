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

#include "fastpair/repository/fast_pair_metadata_fetcher_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "fastpair/internal/test/fast_pair_fake_http_client.h"
#include "internal/network/http_client.h"

namespace nearby {
namespace fastpair {
namespace {

using ::nearby::network::FastPairFakeHttpClient;
using ::nearby::network::Url;
using OsType = ::nearby::api::DeviceInfo::OsType;

constexpr char kResponseProto[] = "result_proto";
constexpr char kRequestUrl[] = "https://googleapis.com/nearbysharing/test";
constexpr char kQueryParameterAlternateOutputKey[] = "alt";
constexpr char kQueryParameterAlternateOutputProto[] = "proto";

class MockHttpClient : public network::HttpClient {
 public:
  ~MockHttpClient() override = default;
};

const FastPairMetadataFetcher::QueryParameters&
GetTestRequestProtoAsQueryParameters() {
  static const FastPairMetadataFetcher::QueryParameters*
      request_as_query_parameters =
          new FastPairMetadataFetcher::QueryParameters(
              {{"key1", "value1_1"}, {"key1", "value1_2"}, {"key2", "value2"}});
  return *request_as_query_parameters;
}

// Adds the key-value pairs of |request_as_query_parameters| as query
// parameters.|request_as_query_parameters| is only non-null.
Url UrlWithQueryParameters(
    absl::string_view url,
    const std::optional<FastPairMetadataFetcher::QueryParameters>&
        request_as_query_parameters) {
  auto url_with_qp = Url::Create(url);
  EXPECT_TRUE(url_with_qp.ok());

  url_with_qp->AddQueryParameter(kQueryParameterAlternateOutputKey,
                                 kQueryParameterAlternateOutputProto);

  if (request_as_query_parameters) {
    for (const auto& key_value : *request_as_query_parameters) {
      url_with_qp->AddQueryParameter(key_value.first, key_value.second);
    }
  }

  return url_with_qp.value();
}

class FastPairMetadataFetcherImplTest : public ::testing::Test {
 protected:
  FastPairMetadataFetcherImplTest() {
    http_client_ = std::make_unique<FastPairFakeHttpClient>();
  }
  void SetUp() override {
    result_ = nullptr;
    network_error_ = nullptr;
  }
  void StartGetUnauthRequest() {
    StartGetUnauthRequestWithRequestAsQueryParameters(
        GetTestRequestProtoAsQueryParameters());
  }
  void OnResult(absl::string_view result) {
    EXPECT_FALSE(result_ || network_error_);
    result_ = std::make_unique<std::string>(result);
  }

  void OnError(FastPairHttpError network_error) {
    EXPECT_FALSE(result_ || network_error_);
    network_error_ = std::make_unique<FastPairHttpError>(network_error);
  }
  void StartGetUnauthRequestWithRequestAsQueryParameters(
      const FastPairMetadataFetcher::QueryParameters&
          request_as_query_parameters) {
    auto url = Url::Create(kRequestUrl);
    ASSERT_TRUE(url.ok());
    flow_.StartGetUnauthRequest(
        url.value(), request_as_query_parameters, http_client_.get(),
        [&](absl::string_view response) { OnResult(response); },
        [&](FastPairHttpError error) { OnError(error); });

    CheckFastPairRepositoryGetUnauthRequest(request_as_query_parameters);
  }
  void CheckPlatformTypeHeader(
      const absl::flat_hash_map<std::string, std::vector<std::string>>&
          headers) {
    auto platform_type =
        GetHeaderFirstValue("X-Sharing-Platform-Type", headers);
    EXPECT_EQ("OSType.CHROME_OS", platform_type);
  }
  std::string GetHeaderFirstValue(
      absl::string_view key,
      const absl::flat_hash_map<std::string, std::vector<std::string>>&
          headers) {
    auto it = headers.find(key);
    while (it != headers.end()) {
      return it->second[0];
    }
    return "";
  }
  void CheckFastPairRepositoryGetUnauthRequest(
      const FastPairMetadataFetcher::QueryParameters&
          request_as_query_parameters) {
    FastPairFakeHttpClient* fake_http_client =
        reinterpret_cast<FastPairFakeHttpClient*>(http_client_.get());
    EXPECT_EQ(fake_http_client->GetPendingRequest().size(), 1);
    const network::HttpRequest& request =
        fake_http_client->GetPendingRequest()[0].request;

    CheckPlatformTypeHeader(request.GetAllHeaders());

    EXPECT_EQ(UrlWithQueryParameters(kRequestUrl, request_as_query_parameters),
              request.GetUrl());

    EXPECT_EQ("GET", request.GetMethodString());

    // Expect no body.
    auto body = request.GetBody();
    EXPECT_TRUE(body.empty());
    auto content_type =
        GetHeaderFirstValue("Content-Type", request.GetAllHeaders());
    EXPECT_EQ("", content_type);
  }
  void CompleteGetUnauthRequest(
      int error,
      std::optional<network::HttpStatusCode> response_code = std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt) {
    FastPairFakeHttpClient* client =
        reinterpret_cast<FastPairFakeHttpClient*>(http_client_.get());
    client->CompleteRequest(error, response_code, response_string);
    EXPECT_TRUE(result_ || network_error_);
  }
  std::unique_ptr<std::string> result_;
  std::unique_ptr<FastPairHttpError> network_error_;

 private:
  std::unique_ptr<network::HttpClient> http_client_;
  FastPairMetadataFetcherImpl flow_{api::DeviceInfo::OsType::kChromeOs};
};

TEST_F(FastPairMetadataFetcherImplTest, GetUnauthRequestSuccess) {
  StartGetUnauthRequest();
  CompleteGetUnauthRequest(0, network::HttpStatusCode::kHttpOk, kResponseProto);
  EXPECT_EQ(*result_, kResponseProto);
  EXPECT_FALSE(network_error_);
}

TEST_F(FastPairMetadataFetcherImplTest, GetUnauthRequestFailure) {
  StartGetUnauthRequest();
  CompleteGetUnauthRequest(0xff00);
  EXPECT_FALSE(result_);
  EXPECT_EQ(*network_error_, FastPairHttpError::kHttpErrorOffline);
}

TEST_F(FastPairMetadataFetcherImplTest, GetUnauthRequestStatus500) {
  StartGetUnauthRequest();
  CompleteGetUnauthRequest(0, network::HttpStatusCode::kHttpInternalServerError,
                           "Fast Pair Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ(*network_error_, FastPairHttpError::kHttpErrorInternal);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
