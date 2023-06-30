// Copyright 2021 Google LLC
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

#include "internal/test/fake_http_client.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/network/http_request.h"
#include "internal/network/http_response.h"
#include "internal/network/http_status_code.h"
#include "internal/network/url.h"

namespace nearby {
namespace network {
namespace {

class FakekHttpClientTest : public ::testing::Test {
 public:
  void StartRequest(
      absl::string_view request_url,
      std::function<void(const absl::StatusOr<HttpResponse>&)> callback) {
    HttpRequest request;
    Url url;
    url.SetUrlPath(request_url);
    request.SetUrl(url);
    request.SetMethod(HttpRequestMethod::kPost);
    request.SetBody("request body");
    client_.StartRequest(request, callback);
  }

  void CompleteRequest(
      HttpStatusCode status, std::string status_message = "",
      absl::flat_hash_map<std::string, std::vector<std::string>> headers = {},
      std::string body = "", int pos = 0) {
    HttpResponse response;
    response.SetStatusCode(status);
    response.SetReasonPhrase(status_message);
    response.SetHeaders(headers);
    response.SetBody(body);
    client_.CompleteRequest(response, pos);
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

 protected:
  FakeHttpClient client_;
};

TEST_F(FakekHttpClientTest, TestMockHttpResponse) {
  absl::StatusOr<HttpResponse> response;
  StartRequest(
      "http://www.google.com",
      [&response](const absl::StatusOr<HttpResponse>& res) { response = res; });

  CompleteRequest(HttpStatusCode::kHttpOk);
  ASSERT_OK(response);
  EXPECT_EQ(response->GetStatusCode(), HttpStatusCode::kHttpOk);
}

TEST_F(FakekHttpClientTest, TestGetResponse) {
  // Set up HttpRequest
  HttpRequest request;
  Url url;
  url.SetUrlPath("http://www.google.com");
  request.SetUrl(url);
  request.SetMethod(HttpRequestMethod::kGet);
  request.SetBody("request body");
  // Set up HttpResponse
  HttpResponse response;
  response.SetStatusCode(HttpStatusCode::kHttpOk);
  client_.SetResponseForSyncRequest(response);
  // Sync GetResponse
  absl::StatusOr<HttpResponse> result = client_.GetResponse(request);

  ASSERT_OK(result);
  EXPECT_EQ(result->GetStatusCode(), HttpStatusCode::kHttpOk);
}

TEST_F(FakekHttpClientTest, TestSystemError) {
  absl::StatusOr<HttpResponse> response;
  StartRequest(
      "http://www.google.com",
      [&response](const absl::StatusOr<HttpResponse>& res) { response = res; });

  CompleteRequest(0xffff);
  ASSERT_FALSE(response.ok());
  EXPECT_EQ(response.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(FakekHttpClientTest, TestHttpErrorCodes) {
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

TEST_F(FakekHttpClientTest, TestCompleteNotExistingRequest) {
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
