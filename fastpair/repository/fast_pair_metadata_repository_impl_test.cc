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

#include "fastpair/repository/fast_pair_metadata_repository_impl.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/common/fast_pair_http_result.h"
#include "fastpair/internal/test/fast_pair_fake_http_client.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fast_pair_metadata_fetcher.h"
#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "internal/network/url.h"

namespace nearby {
namespace fastpair {

using ::nearby::network::HttpClient;
using ::nearby::network::Url;

const char kImageUrl[] = "https://example.com/image.jpg";
const char kListDevicePath[] = "device/";
const char kClientId[] = "AIzaSyBv7ZrOlX5oIJLVQrZh-WkZFKm5L6FlStQ";
const char kTestGoogleApisUrl[] = "https://nearbydevices-pa.googleapis.com";
constexpr int64_t kDeviceId = 10148625;
const char kDeviceName[] = "devicename";

class FakeFastPairMetadataFetcher : public FastPairMetadataFetcher {
 public:
  FakeFastPairMetadataFetcher() = default;
  ~FakeFastPairMetadataFetcher() override = default;
  FakeFastPairMetadataFetcher(const FakeFastPairMetadataFetcher&) = delete;
  FakeFastPairMetadataFetcher& operator=(const FakeFastPairMetadataFetcher&) =
      delete;

  void StartGetUnauthRequest(const Url& request_url,
                             const QueryParameters& request_as_query_parameters,
                             HttpClient* http_client,
                             ResultCallback result_callback,
                             ErrorCallback error_callback) override {
    request_url_ = request_url;
    request_as_query_parameters_ = request_as_query_parameters;
    http_client_ = http_client;
    result_callback_ = result_callback;
    error_callback_ = error_callback;
  }
  Url request_url_;
  QueryParameters request_as_query_parameters_;
  HttpClient* http_client_;
  ResultCallback result_callback_;
  ErrorCallback error_callback_;
};
std::vector<std::string> ExpectQueryStringValues(
    const FastPairMetadataFetcher::QueryParameters& query_parameters,
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
class FastPairMetadataRepositoryImplTest : public ::testing::Test {
 protected:
  FastPairMetadataRepositoryImplTest() = default;

  void SetUp() override {
    http_client_ = std::make_unique<network::FastPairFakeHttpClient>();
    SetNearbySharedHttpHost(kTestGoogleApisUrl);

    auto fetcher = std::make_unique<FakeFastPairMetadataFetcher>();
    fetcher_ = fetcher.get();

    repository_ = std::make_unique<FastPairMetadataRepositoryImpl>(
        std::move(fetcher), std::move(http_client_));
  }

  void SetNearbySharedHttpHost(const std::string& host) {
    int global_host_size = 0;
    char global_host[256];
    if (host.size() > 256) {
      return;
    }
    global_host_size = host.size();
    memcpy(global_host, host.c_str(), global_host_size);
  }

  const FastPairMetadataFetcher::QueryParameters&
  request_as_query_parameters() {
    return fetcher_->request_as_query_parameters_;
  }

  const Url& request_url() { return fetcher_->request_url_; }

  Url GetUrl(absl::string_view path) {
    return Url::Create(
               absl::StrCat(kTestGoogleApisUrl, "/v1/", kListDevicePath, path))
        .value();
  }

  FakeFastPairMetadataFetcher* fetcher_;
  std::unique_ptr<HttpClient> http_client_;
  std::unique_ptr<FastPairMetadataRepository> repository_;

  Url request_url_;
  FastPairMetadataFetcher::QueryParameters request_as_query_parameters_;
  FastPairMetadataFetcher::ResultCallback result_callback_;
  FastPairMetadataFetcher::ErrorCallback error_callback_;
};

TEST_F(FastPairMetadataRepositoryImplTest, GetObservedDeviceSuccess) {
  proto::GetObservedDeviceRequest request;
  proto::GetObservedDeviceResponse result;

  request.set_device_id(kDeviceId);
  request.set_mode(proto::GetObservedDeviceRequest::MODE_RELEASE);

  repository_->GetObservedDevice(
      request,
      [&result](const proto::GetObservedDeviceResponse& response) {
        result = response;
      },
      [&](FastPairHttpError error) {});

  EXPECT_EQ(request_url(), GetUrl(std::to_string(request.device_id())));
  EXPECT_EQ(std::vector<std::string>{"MODE_RELEASE"},
            ExpectQueryStringValues(request_as_query_parameters(), "mode"));
  EXPECT_EQ(std::vector<std::string>{kClientId},
            ExpectQueryStringValues(request_as_query_parameters(), "key"));

  proto::GetObservedDeviceResponse test_response;
  test_response.mutable_device()->set_id(kDeviceId);
  test_response.mutable_device()->set_name(kDeviceName);
  test_response.mutable_device()->set_image_url(kImageUrl);

  EXPECT_EQ(test_response.device().id(), kDeviceId);
  EXPECT_EQ(test_response.device().name(), kDeviceName);
  EXPECT_EQ(test_response.device().image_url(), kImageUrl);
}

}  // namespace fastpair
}  // namespace nearby
