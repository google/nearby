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

#include "fastpair/internal/test/fast_pair_fake_http_client.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "internal/network/http_status_code.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace network {
namespace {

class FastPairFakeHttpClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    client_ = FastPairFakeHttpClient();
    response_ = HttpResponse();
    request_ = HttpRequest();
  }

  void StartRequest(
      absl::string_view request_url,
      std::function<void(const absl::StatusOr<HttpResponse>&)> callback) {
    Url url;
    url.SetUrlPath(request_url);
    request_.SetUrl(std::move(url));
    request_.SetMethod(HttpRequestMethod::kPost);
    request_.SetBody("request body");
    client_.StartRequest(request_, absl::Seconds(10), callback);
  }

  void CompleteRequest(
      HttpStatusCode status, std::string status_message = "",
      absl::flat_hash_map<std::string, std::vector<std::string>> headers = {},
      std::string body = "", int pos = 0) {
    response_.SetStatusCode(status);
    response_.SetReasonPhrase(status_message);
    response_.SetHeaders(headers);
    response_.SetBody(body);
    client_.CompleteRequest(response_, pos);
  }

  void CompleteRequest(int error, int status = 200, int pos = 0) {
    client_.CompleteRequest(error, static_cast<HttpStatusCode>(status),
                            std::nullopt, pos);
  }

  void CheckHttpError(int status, absl::StatusCode expected_status) {
    absl::StatusOr<HttpResponse> response;
    StartRequest("http://www.google.com",
                 [&response](const absl::StatusOr<HttpResponse>& res) {
                   response = res;
                 });

    client_.CompleteRequest(0, static_cast<HttpStatusCode>(status),
                            std::nullopt);
    if (status >= 200 & status < 300) {
      ASSERT_TRUE(response.ok());
    } else {
      ASSERT_FALSE(response.ok());
    }

    EXPECT_EQ(response.status().code(), expected_status);
  }

 private:
  FastPairFakeHttpClient client_;
  HttpResponse response_;
  HttpRequest request_;
};

TEST_F(FastPairFakeHttpClientTest, TestMockHttpResponse) {
  absl::StatusOr<HttpResponse> response;
  StartRequest(
      "http://www.google.com",
      [&response](const absl::StatusOr<HttpResponse>& res) { response = res; });

  CompleteRequest(HttpStatusCode::kHttpOk);
  ASSERT_TRUE(response.ok());
  EXPECT_EQ(response->GetStatusCode(), HttpStatusCode::kHttpOk);
}

TEST_F(FastPairFakeHttpClientTest, TestSystemError) {
  absl::StatusOr<HttpResponse> response;
  StartRequest(
      "http://www.google.com",
      [&response](const absl::StatusOr<HttpResponse>& res) { response = res; });

  CompleteRequest(0xffff);
  ASSERT_FALSE(response.ok());
  EXPECT_EQ(response.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(FastPairFakeHttpClientTest, TestHttpErrorCodes) {
  CheckHttpError(200, absl::StatusCode::kOk);
  CheckHttpError(400, absl::StatusCode::kInvalidArgument);
  CheckHttpError(401, absl::StatusCode::kUnauthenticated);
  CheckHttpError(403, absl::StatusCode::kPermissionDenied);
  CheckHttpError(404, absl::StatusCode::kNotFound);
  CheckHttpError(409, absl::StatusCode::kAborted);
  CheckHttpError(416, absl::StatusCode::kOutOfRange);
  CheckHttpError(429, absl::StatusCode::kResourceExhausted);
  CheckHttpError(499, absl::StatusCode::kCancelled);
  CheckHttpError(504, absl::StatusCode::kDeadlineExceeded);
  CheckHttpError(501, absl::StatusCode::kUnimplemented);
  CheckHttpError(503, absl::StatusCode::kUnavailable);
  CheckHttpError(488, absl::StatusCode::kFailedPrecondition);
  CheckHttpError(522, absl::StatusCode::kInternal);
  CheckHttpError(777, absl::StatusCode::kUnknown);
}

TEST_F(FastPairFakeHttpClientTest, TestCompleteNotExistingRequest) {
  int count = 0;
  StartRequest("http://www.google.com",
               [&count](const absl::StatusOr<HttpResponse>& res) { ++count; });

  CompleteRequest(0xffff);
  EXPECT_EQ(count, 1);
  CompleteRequest(0xffff);
  EXPECT_EQ(count, 1);
}

}  // namespace
}  // namespace network
}  // namespace nearby
